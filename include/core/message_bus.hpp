#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "message.hpp"

namespace keystone {

// Forward declarations
namespace agents {
class AgentBase;
}

namespace concurrency {
class WorkStealingScheduler;
}

namespace core {

/**
 * @brief Central message routing hub for agent communication
 *
 * MessageBus decouples agents from each other, enabling:
 * - Dynamic agent registration/discovery
 * - Automatic message routing by agent ID
 * - Async message processing via WorkStealingScheduler (Phase A Week 3+)
 * - Backward compatibility with synchronous routing
 *
 * Architecture:
 * - Without scheduler: Synchronous routing (Phase 1-3 behavior)
 * - With scheduler: Async routing via work-stealing threads
 */
class MessageBus {
 public:
  MessageBus() = default;
  ~MessageBus() = default;

  // Non-copyable, non-movable
  MessageBus(const MessageBus&) = delete;
  MessageBus& operator=(const MessageBus&) = delete;
  MessageBus(MessageBus&&) = delete;
  MessageBus& operator=(MessageBus&&) = delete;

  /**
   * @brief Set the work-stealing scheduler for async message routing
   *
   * When scheduler is set, routeMessage() submits messages as work items
   * to the scheduler instead of synchronous delivery.
   *
   * @param scheduler Pointer to scheduler (must outlive MessageBus)
   */
  void setScheduler(concurrency::WorkStealingScheduler* scheduler);

  /**
   * @brief Get the current scheduler (may be nullptr)
   */
  concurrency::WorkStealingScheduler* getScheduler() const;

  /**
   * @brief Register an agent with the bus
   *
   * @param agent_id Unique identifier for the agent
   * @param agent Pointer to the agent (must outlive registration)
   * @throws std::runtime_error if agent_id already registered
   */
  void registerAgent(const std::string& agent_id, agents::AgentBase* agent);

  /**
   * @brief Unregister an agent from the bus
   *
   * @param agent_id Agent identifier to unregister
   */
  void unregisterAgent(const std::string& agent_id);

  /**
   * @brief Route a message to the appropriate agent
   *
   * Behavior depends on scheduler:
   * - No scheduler: Synchronous delivery via agent->receiveMessage()
   * - With scheduler: Async delivery via scheduler->submit()
   *
   * @param msg Message to route (uses msg.receiver_id for routing)
   * @return true if message was delivered/submitted, false if receiver not
   * found
   */
  bool routeMessage(const KeystoneMessage& msg);

  /**
   * @brief Check if an agent is registered
   *
   * @param agent_id Agent identifier to check
   * @return true if agent is registered
   */
  bool hasAgent(const std::string& agent_id) const;

  /**
   * @brief Get list of all registered agent IDs
   *
   * @return std::vector<std::string> List of agent IDs
   */
  std::vector<std::string> listAgents() const;

 private:
  mutable std::mutex registry_mutex_;
  std::unordered_map<std::string, agents::AgentBase*> agents_;
  // FIX C5: Atomic scheduler pointer for thread-safe access without registry_mutex
  std::atomic<concurrency::WorkStealingScheduler*> scheduler_{nullptr};
};

}  // namespace core
}  // namespace keystone
