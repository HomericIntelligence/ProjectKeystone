#include "agents/agent_base.hpp"
#include "core/message_bus.hpp"
#include <stdexcept>

namespace keystone {
namespace agents {

AgentBase::AgentBase(const std::string& agent_id)
    : agent_id_(agent_id), inbox_() {
}

void AgentBase::sendMessage(const core::KeystoneMessage& msg) {
    if (!message_bus_) {
        throw std::runtime_error("Message bus not set for agent: " + agent_id_);
    }

    message_bus_->routeMessage(msg);
}

void AgentBase::receiveMessage(const core::KeystoneMessage& msg) {
    // Lock-free enqueue - no mutex needed!
    inbox_.enqueue(msg);
}

std::optional<core::KeystoneMessage> AgentBase::getMessage() {
    // Lock-free dequeue - no mutex needed!
    core::KeystoneMessage msg;
    if (inbox_.try_dequeue(msg)) {
        return msg;
    }
    return std::nullopt;
}

void AgentBase::setMessageBus(core::MessageBus* bus) {
    message_bus_ = bus;
}

} // namespace agents
} // namespace keystone
