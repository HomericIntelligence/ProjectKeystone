#include "agents/chief_architect_agent.hpp"

namespace keystone {
namespace agents {

ChiefArchitectAgent::ChiefArchitectAgent(const std::string& agent_id)
    : BaseAgent(agent_id) {
}

core::Response ChiefArchitectAgent::processMessage(const core::KeystoneMessage& msg) {
    // For Phase 1, ChiefArchitect receives responses from TaskAgent
    // Store the response for retrieval
    if (msg.payload) {
        core::Response resp = core::Response::createSuccess(msg, agent_id_, *msg.payload);
        storeResponse(resp);
        return resp;
    }

    return core::Response::createError(msg, agent_id_, "No payload in message");
}

core::Response ChiefArchitectAgent::sendCommand(const std::string& command, BaseAgent* task_agent) {
    // Create message
    auto msg = core::KeystoneMessage::create(
        agent_id_,
        task_agent->getAgentId(),
        command
    );

    // Send to task agent
    sendMessage(msg, task_agent);

    // For Phase 1 synchronous testing, task agent processes immediately
    // and sends response back
    // Get the response
    auto response_msg = getMessage();
    if (response_msg) {
        return processMessage(*response_msg);
    }

    return core::Response::createError(msg, agent_id_, "No response received");
}

} // namespace agents
} // namespace keystone
