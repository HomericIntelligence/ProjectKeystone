#include "agents/chief_architect_agent.hpp"

namespace keystone {
namespace agents {

ChiefArchitectAgent::ChiefArchitectAgent(const std::string& agent_id)
    : BaseAgent(agent_id) {
}

core::Response ChiefArchitectAgent::processMessage(const core::KeystoneMessage& msg) {
    // For Phase 1, ChiefArchitect receives responses from TaskAgent
    // Defensive: check payload exists before dereferencing
    if (!msg.payload) {
        return core::Response::createError(msg, agent_id_, "Missing payload in message");
    }

    core::Response resp = core::Response::createSuccess(msg, agent_id_, *msg.payload);
    return resp;
}

core::Response ChiefArchitectAgent::sendCommand(const std::string& command, const std::string& task_agent_id) {
    // Create message
    auto msg = core::KeystoneMessage::create(
        agent_id_,
        task_agent_id,
        command
    );

    // Send to task agent via MessageBus
    sendMessage(msg);

    // For Phase 1 synchronous testing, task agent processes immediately
    // and sends response back via MessageBus
    // Get the response
    auto response_msg = getMessage();
    if (response_msg) {
        return processMessage(*response_msg);
    }

    return core::Response::createError(msg, agent_id_, "No response received");
}

} // namespace agents
} // namespace keystone
