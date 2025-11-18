#include "agents/task_agent.hpp"
#include "core/metrics.hpp"
#include <cstdio>
#include <array>
#include <stdexcept>
#include <sstream>

namespace keystone {
namespace agents {

TaskAgent::TaskAgent(const std::string& agent_id)
    : BaseAgent(agent_id) {
}

core::Response TaskAgent::processMessage(const core::KeystoneMessage& msg) {
    // FIX: Record message processed for metrics tracking
    core::Metrics::getInstance().recordMessageProcessed(msg.msg_id);

    // FIX: Check if deadline was missed
    if (msg.deadline.has_value() && msg.hasDeadlinePassed()) {
        auto time_remaining = msg.getTimeUntilDeadline();
        // Deadline passed - time_remaining will be 0, we want the absolute value
        // For now, record as 0ms late (we don't track when it was supposed to be processed)
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
            "response",
            result
        );
        response_msg.msg_id = msg.msg_id;  // Keep same msg_id for tracking

        // MessageBus routes it automatically
        sendMessage(response_msg);

        return response;

    } catch (const std::exception& e) {
        // Create error response
        auto response = core::Response::createError(msg, agent_id_, e.what());

        // Send error response back via MessageBus
        auto response_msg = core::KeystoneMessage::create(
            agent_id_,
            msg.sender_id,
            "response",
            std::string("ERROR: ") + e.what()
        );
        response_msg.msg_id = msg.msg_id;

        sendMessage(response_msg);

        return response;
    }
}

std::string TaskAgent::executeBash(const std::string& command) {
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

} // namespace agents
} // namespace keystone
