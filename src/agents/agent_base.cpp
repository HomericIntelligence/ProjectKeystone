#include "agents/agent_base.hpp"

#include "core/config.hpp"  // FIX m3: Centralized configuration
#include "core/message_bus.hpp"
#include "core/metrics.hpp"

#include <iostream>  // FIX M1: For std::cerr (backpressure logging)
#include <stdexcept>

namespace keystone {
namespace agents {

AgentBase::AgentBase(const std::string& agent_id)
    : agent_id_(agent_id), last_low_priority_check_(std::chrono::steady_clock::now()) {
  // FIX M2: Initialize time-based fairness timer
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
  size_t total_depth = high_priority_inbox_.size_approx() + normal_priority_inbox_.size_approx() +
                       low_priority_inbox_.size_approx();

  if (total_depth >= core::Config::AGENT_MAX_QUEUE_SIZE) {
    // Apply backpressure: reject message to prevent memory exhaustion
    if (!backpressure_applied_.exchange(true)) {
      // Log warning on first occurrence
      std::cerr << "[BACKPRESSURE] Agent " << agent_id_ << " inbox full (" << total_depth
                << " messages), "
                << "rejecting new messages" << std::endl;
    }

    // For now, drop the message (could also throw exception or send NACK)
    // In production, sender should retry with exponential backoff
    return;
  }

  // Clear backpressure flag if queue is below limit
  size_t low_watermark = static_cast<size_t>(core::Config::AGENT_MAX_QUEUE_SIZE *
                                             core::Config::AGENT_QUEUE_LOW_WATERMARK_PERCENT);
  if (total_depth < low_watermark) {
    if (backpressure_applied_.exchange(false)) {
      std::cerr << "[BACKPRESSURE] Agent " << agent_id_ << " inbox recovered (" << total_depth
                << " messages), "
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
  // FIX M2: Time-based priority fairness to prevent LOW priority starvation
  // Strategy: Every Config::AGENT_LOW_PRIORITY_CHECK_INTERVAL, force-check NORMAL/LOW
  // queues to ensure fairness even under sustained HIGH priority load

  core::KeystoneMessage msg;
  auto now = std::chrono::steady_clock::now();

  // Every 100ms, force-check lower priorities regardless of HIGH queue state
  if (now - last_low_priority_check_ >= core::Config::AGENT_LOW_PRIORITY_CHECK_INTERVAL) {
    last_low_priority_check_ = now;

    // Try NORMAL first (give it priority over LOW during fairness check)
    if (normal_priority_inbox_.try_dequeue(msg)) {
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
  size_t total_depth = high_priority_inbox_.size_approx() + normal_priority_inbox_.size_approx() +
                       low_priority_inbox_.size_approx();

  core::Metrics::getInstance().recordQueueDepth(agent_id_, total_depth);
}

void AgentBase::setMessageBus(core::MessageBus* bus) {
  message_bus_ = bus;
}

}  // namespace agents
}  // namespace keystone
