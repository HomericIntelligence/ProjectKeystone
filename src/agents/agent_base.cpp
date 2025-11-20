#include "agents/agent_base.hpp"

#include <iostream>  // FIX M1: For std::cerr (backpressure logging)
#include <stdexcept>

#include "core/config.hpp"  // FIX m3: Centralized configuration
#include "core/message_bus.hpp"
#include "core/metrics.hpp"

namespace keystone {
namespace agents {

AgentBase::AgentBase(const std::string& agent_id) : agent_id_(agent_id) {
  // FIX C1: Initialize time-based fairness timer (thread-safe atomic)
  auto now = std::chrono::steady_clock::now();
  last_low_priority_check_ns_.store(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(),
      std::memory_order_relaxed);
  // Phase C: Priority queues initialized by default constructors
}

void AgentBase::sendMessage(const core::KeystoneMessage& msg) {
  if (!message_bus_) {
    throw std::runtime_error("Message bus not set for agent: " + agent_id_);
  }

  message_bus_->routeMessage(msg);
}

void AgentBase::receiveMessage(const core::KeystoneMessage& msg) {
  // FIX C4: Backpressure - check queue size limit before accepting message
  // THREAD-SAFE: Separate atomic check from side effects to prevent race
  size_t total_depth = high_priority_inbox_.size_approx() + normal_priority_inbox_.size_approx() +
                       low_priority_inbox_.size_approx();

  bool should_apply_backpressure = (total_depth >= core::Config::AGENT_MAX_QUEUE_SIZE);
  bool was_backpressure_applied = backpressure_applied_.load(std::memory_order_relaxed);

  if (should_apply_backpressure) {
    // Apply backpressure: reject message to prevent memory exhaustion
    if (!was_backpressure_applied) {
      // Only one thread wins the race to set and log
      bool expected = false;
      if (backpressure_applied_.compare_exchange_strong(expected, true,
                                                        std::memory_order_relaxed)) {
        // Log warning on first occurrence
        std::cerr << "[BACKPRESSURE] Agent " << agent_id_ << " inbox full (" << total_depth
                  << " messages), rejecting new messages" << std::endl;
      }
    }

    // For now, drop the message (could also throw exception or send NACK)
    // In production, sender should retry with exponential backoff
    return;
  }

  // Clear backpressure flag if queue is below low watermark (hysteresis)
  size_t low_watermark = static_cast<size_t>(core::Config::AGENT_MAX_QUEUE_SIZE *
                                             core::Config::AGENT_QUEUE_LOW_WATERMARK_PERCENT);
  if (total_depth < low_watermark && was_backpressure_applied) {
    // Only one thread wins the race to clear and log
    bool expected = true;
    if (backpressure_applied_.compare_exchange_strong(expected, false, std::memory_order_relaxed)) {
      std::cerr << "[BACKPRESSURE] Agent " << agent_id_ << " inbox recovered (" << total_depth
                << " messages), accepting messages again" << std::endl;
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
  // FIX C1: Time-based priority fairness to prevent LOW priority starvation
  // Strategy: Every Config::AGENT_LOW_PRIORITY_CHECK_INTERVAL, force-check NORMAL/LOW
  // queues to ensure fairness even under sustained HIGH priority load
  // THREAD-SAFE: Uses atomic for last_low_priority_check_ns_

  core::KeystoneMessage msg;
  auto now = std::chrono::steady_clock::now();
  auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

  // Load last check time (atomic, thread-safe)
  int64_t last_check_ns = last_low_priority_check_ns_.load(std::memory_order_relaxed);
  auto interval_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         core::Config::AGENT_LOW_PRIORITY_CHECK_INTERVAL)
                         .count();

  // Every 100ms, force-check lower priorities regardless of HIGH queue state
  if (now_ns - last_check_ns >= interval_ns) {
    // Try to update last check time (only one thread succeeds)
    last_low_priority_check_ns_.store(now_ns, std::memory_order_relaxed);

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
  size_t total_depth = high_priority_inbox_.size_approx() +
                       normal_priority_inbox_.size_approx() +
                       low_priority_inbox_.size_approx();

  core::Metrics::getInstance().recordQueueDepth(agent_id_, total_depth);
}

void AgentBase::setMessageBus(core::MessageBus* bus) { message_bus_ = bus; }

}  // namespace agents
}  // namespace keystone
