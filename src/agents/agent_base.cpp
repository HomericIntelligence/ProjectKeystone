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
        default:
            // FIX: Invalid priority - route to NORMAL queue as fallback
            normal_priority_inbox_.enqueue(msg);
            break;
    }
}

std::optional<core::KeystoneMessage> AgentBase::getMessage() {
    // FIX: Weighted round-robin to prevent LOW priority starvation
    // Strategy: After processing HIGH_PRIORITY_QUOTA high-priority messages,
    // force-check NORMAL and LOW queues to ensure fairness

    core::KeystoneMessage msg;
    uint64_t high_count = high_priority_processed_.load(std::memory_order_relaxed);

    // Every HIGH_PRIORITY_QUOTA messages, give lower priorities a chance
    if (high_count >= HIGH_PRIORITY_QUOTA) {
        // Reset counter
        high_priority_processed_.store(0, std::memory_order_relaxed);

        // Try NORMAL first (give it priority over LOW)
        if (normal_priority_inbox_.try_dequeue(msg)) {
            return msg;
        }

        // Then try LOW
        if (low_priority_inbox_.try_dequeue(msg)) {
            return msg;
        }

        // Fall through to HIGH if NORMAL/LOW are empty
    }

    // Standard priority order: HIGH -> NORMAL -> LOW
    if (high_priority_inbox_.try_dequeue(msg)) {
        high_priority_processed_.fetch_add(1, std::memory_order_relaxed);
        return msg;
    }

    if (normal_priority_inbox_.try_dequeue(msg)) {
        return msg;
    }

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
