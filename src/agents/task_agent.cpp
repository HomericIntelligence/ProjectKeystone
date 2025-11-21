#include "agents/task_agent.hpp"

#include <array>
#include <cstdio>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "core/error_sanitizer.hpp"
#include "core/metrics.hpp"

namespace keystone {
namespace agents {

// FIX P1-03: Whitelist of allowed commands to prevent command injection
// This is a security-critical list - only safe, non-destructive commands allowed
static const std::unordered_set<std::string> ALLOWED_COMMANDS = {
    "echo",    // Print text
    "cat",     // Display file contents
    "ls",      // List directory
    "pwd",     // Print working directory
    "date",    // Show date/time
    "whoami",  // Show current user
    "uname",   // Show system info
    "head",    // Show first lines of file
    "tail",    // Show last lines of file
    "wc",      // Word count
    "grep",    // Search text
    "find",    // Find files
    "bc"       // Calculator (for test arithmetic)
};

TaskAgent::TaskAgent(const std::string& agent_id) : BaseAgent(agent_id) {}

concurrency::Task<core::Response> TaskAgent::processMessage(const core::KeystoneMessage& msg) {
  // FIX C3: Changed to async (returns Task<Response>)
  // Record message processed for metrics tracking
  core::Metrics::getInstance().recordMessageProcessed(msg.msg_id);

  // Check if deadline was missed
  if (msg.deadline.has_value() && msg.hasDeadlinePassed()) {
    auto time_remaining = msg.getTimeUntilDeadline();
    (void)time_remaining;  // Suppress unused warning - reserved for future use
    // Deadline passed - time_remaining will be 0, we want the absolute value
    // For now, record as 0ms late (we don't track when it was supposed to be
    // processed)
    core::Metrics::getInstance().recordDeadlineMiss(msg.msg_id, 0);
  }

  try {
    // Execute the bash command
    std::string result = executeBash(msg.command);

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

    co_return response;  // FIX C3: Use co_return for coroutine

  } catch (const std::exception& e) {
    // FIX P3-08: Sanitize error message to prevent information disclosure
    std::string sanitized_error = core::sanitizeErrorMessage(e.what());

    // Create error response with sanitized message
    auto response = core::Response::createError(msg, agent_id_, sanitized_error);

    // Send error response back via MessageBus
    auto response_msg =
        core::KeystoneMessage::create(agent_id_, msg.sender_id, "response",
                                      std::string("ERROR: ") + sanitized_error);
    response_msg.msg_id = msg.msg_id;

    sendMessage(response_msg);

    co_return response;  // FIX C3: Use co_return for coroutine
  }
}

void TaskAgent::validateCommand(const std::string& command) {
  // FIX P1-03: Security validation to prevent command injection attacks

  // 1. Check for empty command
  if (command.empty()) {
    throw std::runtime_error("Command cannot be empty");
  }

  // 2. Check for dangerous shell metacharacters that enable command chaining/injection
  // These characters allow attackers to execute additional commands
  const std::string dangerous_chars = ";|&$`<>(){}[]!";
  if (command.find_first_of(dangerous_chars) != std::string::npos) {
    throw std::runtime_error(
        "Command contains disallowed shell metacharacters. "
        "Forbidden characters: " + dangerous_chars);
  }

  // 3. Check for command substitution attempts
  if (command.find("$(") != std::string::npos ||
      command.find("`") != std::string::npos) {
    throw std::runtime_error("Command substitution not allowed");
  }

  // 4. Extract the base command (first token)
  std::istringstream iss(command);
  std::string base_command;
  iss >> base_command;

  // 5. Validate against whitelist
  if (ALLOWED_COMMANDS.find(base_command) == ALLOWED_COMMANDS.end()) {
    std::stringstream ss;
    ss << "Command not in whitelist: '" << base_command << "'. ";
    ss << "Allowed commands: ";
    bool first = true;
    for (const auto& cmd : ALLOWED_COMMANDS) {
      if (!first) ss << ", ";
      ss << cmd;
      first = false;
    }
    throw std::runtime_error(ss.str());
  }

  // 6. Additional validation: prevent directory traversal in arguments
  if (command.find("..") != std::string::npos) {
    throw std::runtime_error("Directory traversal (..) not allowed in command arguments");
  }
}

std::string TaskAgent::executeBash(const std::string& command) {
  // FIX P1-03: Validate command BEFORE execution to prevent injection
  validateCommand(command);

  std::array<char, 4096> buffer;  // Increased from 128 to handle larger outputs
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
