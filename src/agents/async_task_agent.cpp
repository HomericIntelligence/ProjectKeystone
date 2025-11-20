#include "agents/async_task_agent.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <future>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "core/metrics.hpp"

namespace keystone {
namespace agents {

AsyncTaskAgent::AsyncTaskAgent(const std::string& agent_id)
    : AsyncBaseAgent(agent_id) {}

concurrency::Task<core::Response> AsyncTaskAgent::processMessage(
    const core::KeystoneMessage& msg) {
  // FIX: Record message processed for metrics tracking
  core::Metrics::getInstance().recordMessageProcessed(msg.msg_id);

  // FIX: Check if deadline was missed
  if (msg.deadline.has_value() && msg.hasDeadlinePassed()) {
    auto time_remaining = msg.getTimeUntilDeadline();
    (void)time_remaining;  // Suppress unused warning - reserved for future use
    // Deadline passed - time_remaining will be 0, we want the absolute value
    // For now, record as 0ms late (we don't track when it was supposed to be
    // processed)
    core::Metrics::getInstance().recordDeadlineMiss(msg.msg_id, 0);
  }

  try {
    // Skip response messages (avoid infinite loop)
    if (msg.command == "response") {
      // Just create a success response without executing
      auto response =
          core::Response::createSuccess(msg, agent_id_, "response received");
      co_return response;
    }

    // Execute bash command asynchronously - this suspends the coroutine
    std::string result = co_await executeBashAsync(msg.command);

    // Log the execution
    command_log_.emplace_back(msg.command, result);

    // Create success response
    auto response = core::Response::createSuccess(msg, agent_id_, result);

    // Send response back to sender via MessageBus
    auto response_msg = core::KeystoneMessage::create(
        agent_id_,
        msg.sender_id,  // Route back to original sender
        "response", result);
    response_msg.msg_id = msg.msg_id;  // Keep same msg_id for tracking

    // MessageBus routes it automatically
    sendMessage(response_msg);

    co_return response;

  } catch (const std::exception& e) {
    // Create error response
    auto response = core::Response::createError(msg, agent_id_, e.what());

    // Send error response back via MessageBus
    auto response_msg =
        core::KeystoneMessage::create(agent_id_, msg.sender_id, "response",
                                      std::string("ERROR: ") + e.what());
    response_msg.msg_id = msg.msg_id;

    sendMessage(response_msg);

    co_return response;
  }
}

concurrency::Task<std::string> AsyncTaskAgent::executeBashAsync(
    const std::string& command) {
  // For Phase B initial implementation, execute synchronously within the
  // coroutine This still prevents blocking the calling thread (MessageBus)
  // since we're already running on a worker thread from the scheduler
  //
  // The key benefit: The MessageBus thread returns immediately after submit(),
  // and the actual bash execution happens on a worker thread
  //
  // TODO Phase C: Implement true async I/O using non-blocking pipes
  std::string result = executeBashSync(command);
  co_return result;
}

std::string AsyncTaskAgent::executeBashSync(const std::string& command) {
  std::array<char, 4096> buffer;
  std::string result;

  // RAII pipe handle - automatically closes on exception or return
  PipeHandle pipe(popen(command.c_str(), "r"));
  if (!pipe) {
    throw std::runtime_error("popen() failed");
  }

  // Read command output
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }

  // Manual close to get status (release() transfers ownership)
  int status = pclose(pipe.release());
  if (status != 0) {
    std::stringstream ss;
    ss << "Command exited with status " << status;
    throw std::runtime_error(ss.str());
  }

  // Remove trailing newline if present
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }

  return result;
}

}  // namespace agents
}  // namespace keystone
