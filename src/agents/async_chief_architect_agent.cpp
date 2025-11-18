#include "agents/async_chief_architect_agent.hpp"

namespace keystone {
namespace agents {

AsyncChiefArchitectAgent::AsyncChiefArchitectAgent(const std::string& agent_id)
    : AsyncBaseAgent(agent_id) {
}

concurrency::Task<core::Response> AsyncChiefArchitectAgent::processMessage(const core::KeystoneMessage& msg) {
    // Skip response messages to avoid feedback loops (similar to AsyncTaskAgent)
    if (msg.command == "response") {
        auto response = core::Response::createSuccess(msg, agent_id_, "response received");
        co_return response;
    }

    // ChiefArchitect receives responses from subordinates (TaskAgent, ModuleLead, etc.)
    // Check payload exists
    if (!msg.payload) {
        auto response = core::Response::createError(msg, agent_id_, "Missing payload in message");
        co_return response;
    }

    // Create success response with payload
    auto response = core::Response::createSuccess(msg, agent_id_, *msg.payload);
    co_return response;
}

void AsyncChiefArchitectAgent::sendCommandAsync(const std::string& command, const std::string& subordinate_id) {
    // Create message
    auto msg = core::KeystoneMessage::create(
        agent_id_,
        subordinate_id,
        command
    );

    // Track pending command for response correlation
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_commands_[msg.msg_id] = command;
    }

    // Send via MessageBus (async routing if scheduler set)
    sendMessage(msg);
}

} // namespace agents
} // namespace keystone
