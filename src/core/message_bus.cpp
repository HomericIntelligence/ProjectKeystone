#include "core/message_bus.hpp"
#include "agents/base_agent.hpp"
#include <stdexcept>

namespace keystone {
namespace core {

void MessageBus::registerAgent(const std::string& agent_id, agents::BaseAgent* agent) {
    if (!agent) {
        throw std::invalid_argument("Cannot register null agent");
    }

    std::lock_guard<std::mutex> lock(registry_mutex_);

    if (agents_.find(agent_id) != agents_.end()) {
        throw std::runtime_error("Agent ID already registered: " + agent_id);
    }

    agents_[agent_id] = agent;
}

void MessageBus::unregisterAgent(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    agents_.erase(agent_id);
}

bool MessageBus::routeMessage(const KeystoneMessage& msg) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = agents_.find(msg.receiver_id);
    if (it == agents_.end()) {
        return false;  // Receiver not found
    }

    // Synchronous delivery for Phase 1
    it->second->receiveMessage(msg);
    return true;
}

bool MessageBus::hasAgent(const std::string& agent_id) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return agents_.find(agent_id) != agents_.end();
}

std::vector<std::string> MessageBus::listAgents() const {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    std::vector<std::string> agent_ids;
    agent_ids.reserve(agents_.size());

    for (const auto& [id, _] : agents_) {
        agent_ids.push_back(id);
    }

    return agent_ids;
}

} // namespace core
} // namespace keystone
