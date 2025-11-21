#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "message.hpp"

// Forward declarations (must be outside namespace keystone to avoid nesting)
namespace keystone { namespace agents { class AgentBase; } }
namespace keystone { namespace concurrency { class WorkStealingScheduler; } }

// Include concepts for compile-time agent interface verification
#include "agents/concepts.hpp"

namespace keystone {
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
   * FIX C2: Changed to shared_ptr for safe lifetime management.
   * Prevents use-after-free in async routing scenarios.
   *
   * @param agent_id Unique identifier for the agent
   * @param agent Shared pointer to the agent (lifetime managed by shared_ptr)
   * @throws std::runtime_error if agent_id already registered
   */
  void registerAgent(const std::string& agent_id, std::shared_ptr<agents::AgentBase> agent);

  /**
   * @brief Register an agent with compile-time interface verification (Issue #24)
   *
   * This templated version uses C++20 concepts to verify at compile time
   * that the agent implements the required interface:
   * - getAgentId() -> string
   * - sendMessage(KeystoneMessage) -> void
   * - receiveMessage(KeystoneMessage) -> void
   * - processMessage(KeystoneMessage) -> Task<Response>
   *
   * Benefits:
   * - Compile-time errors for missing methods
   * - Better error messages than SFINAE
   * - Self-documenting code (concept is the contract)
   * - Enables generic agent algorithms
   *
   * Example:
   * @code
   * auto agent = std::make_shared<ChiefArchitectAgent>("chief");
   * bus.registerAgent(agent);  // Compile error if interface incomplete
   * @endcode
   *
   * @tparam A Agent type satisfying the Agent concept
   * @param agent Shared pointer to the agent
   * @throws std::runtime_error if agent_id already registered
   */
  template <agents::Agent A>
  void registerAgent(std::shared_ptr<A> agent) {
    if (!agent) {
      throw std::runtime_error("MessageBus::registerAgent: null agent pointer");
    }

    // Use the agent's ID
    std::string agent_id = agent->getAgentId();

    // Convert to AgentBase pointer for storage
    std::shared_ptr<agents::AgentBase> base_agent = agent;

    // Delegate to the existing implementation
    registerAgent(agent_id, base_agent);
  }

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
  // FIX C2: Use shared_ptr for safe lifetime management in async scenarios
  std::unordered_map<std::string, std::shared_ptr<agents::AgentBase>> agents_;
  // FIX C5: Atomic scheduler pointer for thread-safe access without registry_mutex
  std::atomic<concurrency::WorkStealingScheduler*> scheduler_{nullptr};
};

}  // namespace core
}  // namespace keystone
