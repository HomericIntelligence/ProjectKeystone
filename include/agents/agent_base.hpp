#pragma once

#include <atomic>
#include <chrono>  // FIX M2: For time-based priority fairness
#include <optional>
#include <string>

#include "concurrentqueue.h"
#include "core/config.hpp"  // FIX m3: Centralized configuration
#include "core/message.hpp"

namespace keystone {

// Forward declarations from core and concurrency namespaces
// CRITICAL: These declarations must be INSIDE keystone namespace but OUTSIDE agents namespace
// to prevent C++ namespace collision. If placed before "namespace keystone {}", the compiler
// creates "keystone::core::MessageBus" but the actual class is defined as "keystone::core::MessageBus",
// causing a doubling bug where both "keystone::core::MessageBus" and "keystone::keystone::core::MessageBus"
// exist in the symbol table. By placing them here, we correctly forward-declare the classes in the
// keystone::core and keystone::concurrency namespaces while allowing agent_base.hpp to use them
// without including their headers.
namespace core { class MessageBus; }
namespace concurrency { class WorkStealingScheduler; }

namespace agents {

/**
 * @brief Common base class for all agents (sync and async)
 *
 * Provides inbox management and message routing functionality.
 * Subclasses implement either sync or async message processing.
 *
 * Phase C Enhancement: Uses lock-free concurrent queue for inbox
 * to eliminate contention under high message load.
 */
class AgentBase {
 public:
  /**
   * @brief Construct a new Agent Base
   *
   * @param agent_id Unique identifier for this agent
   */
  explicit AgentBase(const std::string& agent_id);

  virtual ~AgentBase() = default;

  /**
   * @brief Send a message via the message bus
   *
   * @param msg Message to send
   * @throws std::runtime_error if message bus not set
   */
  void sendMessage(const core::KeystoneMessage& msg);

  /**
   * @brief Receive a message into the inbox (called by MessageBus)
   *
   * Lock-free operation - no mutex contention.
   *
   * @param msg Message to receive
   */
  virtual void receiveMessage(const core::KeystoneMessage& msg);

  /**
   * @brief Get the next message from inbox
   *
   * Lock-free operation - no mutex contention.
   *
   * @return std::optional<core::KeystoneMessage> Next message if available
   */
  std::optional<core::KeystoneMessage> getMessage();

  /**
   * @brief Get the agent ID
   *
   * @return const std::string& Agent identifier
   */
  const std::string& getAgentId() const { return agent_id_; }

  /**
   * @brief Set the message bus for this agent
   *
   * @param bus Pointer to message bus (must outlive agent)
   */
  void setMessageBus(core::MessageBus* bus);

  /**
   * @brief Set the work-stealing scheduler for this agent
   *
   * Issue #19: Scheduler integration for async Task<T> execution.
   *
   * Note: This method is provided for API completeness and testing.
   * The actual scheduler is accessed via thread-local storage by Task<T>::await_suspend()
   * when coroutines execute on worker threads.
   *
   * @param scheduler Pointer to scheduler (must outlive agent)
   */
  void setScheduler(concurrency::WorkStealingScheduler* scheduler) {
    // Note: Agents don't store scheduler themselves - it's accessed via thread-local
    // storage. This method exists for API compatibility and future extensions.
    (void)scheduler;  // Suppress unused parameter warning
  }

  /**
   * @brief Configure the low-priority message check interval
   *
   * Issue #23: Per-agent configurable fairness check interval.
   *
   * Controls how frequently this agent force-checks NORMAL/LOW priority queues
   * to prevent starvation under sustained HIGH priority load. Different agents
   * may have different latency requirements:
   * - Latency-sensitive: use shorter intervals (e.g., 10ms-50ms)
   * - Throughput-optimized: use longer intervals (e.g., 200ms-500ms)
   *
   * @param interval Check interval duration (default: 100ms from Config)
   *
   * Example:
   * @code
   * // Low-latency agent (interactive UI updates)
   * agent->setLowPriorityCheckInterval(std::chrono::milliseconds{10});
   *
   * // High-throughput batch processor
   * agent->setLowPriorityCheckInterval(std::chrono::milliseconds{500});
   * @endcode
   */
  void setLowPriorityCheckInterval(std::chrono::milliseconds interval) {
    low_priority_check_interval_ns_.store(
        std::chrono::duration_cast<std::chrono::nanoseconds>(interval).count(),
        std::memory_order_relaxed);
  }

  /**
   * @brief Get the current low-priority check interval
   *
   * @return std::chrono::milliseconds Current check interval
   */
  std::chrono::milliseconds getLowPriorityCheckInterval() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::nanoseconds{low_priority_check_interval_ns_.load(std::memory_order_relaxed)});
  }

 protected:
  /**
   * @brief Update queue depth metrics
   *
   * Calculates total messages across all priority queues and reports to
   * Metrics. Called after each getMessage() operation.
   */
  void updateQueueDepthMetrics();
  std::string agent_id_;
  core::MessageBus* message_bus_{nullptr};

  // Phase C: Priority-based lock-free inboxes
  // Messages are routed to queues by priority and processed HIGH -> NORMAL ->
  // LOW
  moodycamel::ConcurrentQueue<core::KeystoneMessage> high_priority_inbox_;
  moodycamel::ConcurrentQueue<core::KeystoneMessage> normal_priority_inbox_;
  moodycamel::ConcurrentQueue<core::KeystoneMessage> low_priority_inbox_;

  // FIX C1: Anti-starvation using time-based fairness (not count-based)
  // Force-check lower priorities every N milliseconds to prevent starvation
  // under sustained HIGH priority load
  // THREAD-SAFE: Using atomic to prevent data races from concurrent getMessage() calls
  std::atomic<int64_t> last_low_priority_check_ns_;

  // Issue #23: Per-agent configurable fairness interval (defaults to Config value)
  // THREAD-SAFE: Atomic for lock-free read/write from concurrent getMessage() and setter
  std::atomic<int64_t> low_priority_check_interval_ns_;

  // FIX M1: Backpressure - Queue size limits to prevent memory exhaustion
  std::atomic<bool> backpressure_applied_{
      false};  ///< Flag when backpressure is active
};

}  // namespace agents
}  // namespace keystone
