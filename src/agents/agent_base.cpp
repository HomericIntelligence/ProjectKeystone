#include "agents/agent_base.hpp"
#include "core/message_bus.hpp"
#include <stdexcept>

namespace keystone {
namespace agents {

AgentBase::AgentBase(const std::string& agent_id)
    : agent_id_(agent_id) {
    // Phase C: Priority queues initialized by default constructors
}

void AgentBase::sendMessage(const core::KeystoneMessage& msg) {
    if (!message_bus_) {
        throw std::runtime_error("Message bus not set for agent: " + agent_id_);
    }

    message_bus_->routeMessage(msg);
}

void AgentBase::receiveMessage(const core::KeystoneMessage& msg) {
    // Phase C: Route to appropriate priority queue (lock-free)
    switch (msg.priority) {
        case core::Priority::HIGH:
            high_priority_inbox_.enqueue(msg);
            break;
        case core::Priority::NORMAL:
            normal_priority_inbox_.enqueue(msg);
            break;
        case core::Priority::LOW:
            low_priority_inbox_.enqueue(msg);
            break;
    }
}

std::optional<core::KeystoneMessage> AgentBase::getMessage() {
    // Phase C: Check priority queues in order: HIGH -> NORMAL -> LOW
    core::KeystoneMessage msg;

    // Try HIGH priority first
    if (high_priority_inbox_.try_dequeue(msg)) {
        return msg;
    }

    // Then NORMAL priority
    if (normal_priority_inbox_.try_dequeue(msg)) {
        return msg;
    }

    // Finally LOW priority
    if (low_priority_inbox_.try_dequeue(msg)) {
        return msg;
    }

    return std::nullopt;
}

void AgentBase::setMessageBus(core::MessageBus* bus) {
    message_bus_ = bus;
}

} // namespace agents
} // namespace keystone
