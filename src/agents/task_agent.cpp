#include "agents/task_agent.hpp"
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
    try {
        // Execute the bash command
        std::string result = executeBash(msg.command);

        // Log the execution
        command_log_.emplace_back(msg.command, result);

        // Create success response
        auto response = core::Response::createSuccess(msg, agent_id_, result);

        // Send response back to commander
        auto response_msg = core::KeystoneMessage::create(
            agent_id_,
            msg.sender_id,
            "response",
            result
        );
        response_msg.msg_id = msg.msg_id;  // Keep same msg_id for tracking

        // Find commander and send back
        // For Phase 1, we use a simple approach: send message back through inbox
        sendMessage(response_msg, nullptr);  // Will be handled by the test infrastructure

        return response;

    } catch (const std::exception& e) {
        // Create error response
        auto response = core::Response::createError(msg, agent_id_, e.what());

        // Send error response back
        auto response_msg = core::KeystoneMessage::create(
            agent_id_,
            msg.sender_id,
            "response",
            std::string("ERROR: ") + e.what()
        );
        response_msg.msg_id = msg.msg_id;

        sendMessage(response_msg, nullptr);

        return response;
    }
}

std::string TaskAgent::executeBash(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;

    // Open pipe to command
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed");
    }

    // Read command output
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    // Close pipe and get status
    int status = pclose(pipe);
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
