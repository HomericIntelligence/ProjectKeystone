#include "agents/agent_base.hpp"
#include "core/message_bus.hpp"
#include "core/metrics.hpp"
#include <stdexcept>
#include <iostream>  // FIX M1: For std::cerr (backpressure logging)

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
    // FIX M1: Backpressure - check queue size limit before accepting message
    size_t total_depth = high_priority_inbox_.size_approx() +
                        normal_priority_inbox_.size_approx() +
                        low_priority_inbox_.size_approx();

    if (total_depth >= MAX_QUEUE_SIZE) {
        // Apply backpressure: reject message to prevent memory exhaustion
        if (!backpressure_applied_.exchange(true)) {
            // Log warning on first occurrence
            std::cerr << "[BACKPRESSURE] Agent " << agent_id_
                     << " inbox full (" << total_depth << " messages), "
                     << "rejecting new messages" << std::endl;
        }

        // For now, drop the message (could also throw exception or send NACK)
        // In production, sender should retry with exponential backoff
        return;
    }

    // Clear backpressure flag if queue is below limit
    if (total_depth < MAX_QUEUE_SIZE * 0.8) {  // 80% threshold for hysteresis
        if (backpressure_applied_.exchange(false)) {
            std::cerr << "[BACKPRESSURE] Agent " << agent_id_
                     << " inbox recovered (" << total_depth << " messages), "
                     << "accepting messages again" << std::endl;
        }
    }

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
            // FIX: Track queue depth after dequeue
            updateQueueDepthMetrics();
            return msg;
        }

        // Then try LOW
        if (low_priority_inbox_.try_dequeue(msg)) {
            updateQueueDepthMetrics();
            return msg;
        }

        // Fall through to HIGH if NORMAL/LOW are empty
    }

    // Standard priority order: HIGH -> NORMAL -> LOW
    if (high_priority_inbox_.try_dequeue(msg)) {
        high_priority_processed_.fetch_add(1, std::memory_order_relaxed);
        updateQueueDepthMetrics();
        return msg;
    }

    if (normal_priority_inbox_.try_dequeue(msg)) {
        updateQueueDepthMetrics();
        return msg;
    }

    if (low_priority_inbox_.try_dequeue(msg)) {
        updateQueueDepthMetrics();
        return msg;
    }

    return std::nullopt;
}

void AgentBase::updateQueueDepthMetrics() {
    // FIX: Calculate total queue depth across all priority levels
    size_t total_depth = high_priority_inbox_.size_approx() +
                        normal_priority_inbox_.size_approx() +
                        low_priority_inbox_.size_approx();

    core::Metrics::getInstance().recordQueueDepth(agent_id_, total_depth);
}

void AgentBase::setMessageBus(core::MessageBus* bus) {
    message_bus_ = bus;
}

} // namespace agents
} // namespace keystone
