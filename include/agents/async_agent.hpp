#pragma once

#include <string>

#include "agent_core.hpp"
#include "concurrency/task.hpp"
#include "core/message.hpp"

namespace keystone {
namespace agents {

/**
 * @brief Asynchronous agent base class (unified)
 *
 * FIX C3: Single base class for all agents (async by default).
 * All agents inherit from this class and implement async processMessage().
 *
 * This replaces the dual BaseAgent (sync) / AsyncBaseAgent (async) hierarchy
 * with a single async-by-default base class, enabling polymorphic collections
 * and runtime execution model flexibility.
 */
class AsyncAgent : public AgentCore {
 public:
  /**
   * @brief Construct a new Async Agent
   *
   * @param agent_id Unique identifier for this agent
   */
  explicit AsyncAgent(const std::string& agent_id);

  ~AsyncAgent() override = default;

  /**
   * @brief Process an incoming message asynchronously
   *
   * FIX C3: Changed from Response to Task<Response> to make async default.
   * All concrete agents must implement this method and use coroutines.
   *
   * @param msg The message to process
   * @return concurrency::Task<core::Response> Async task that resolves to response
   */
  virtual concurrency::Task<core::Response> processMessage(const core::KeystoneMessage& msg) = 0;
};

}  // namespace agents
}  // namespace keystone
