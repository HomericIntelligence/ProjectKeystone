#pragma once

#include <map>

#include "base_agent.hpp"

namespace keystone {
namespace agents {

/**
 * @brief Level 0 Chief Architect Agent
 *
 * Strategic orchestrator that delegates tasks to lower-level agents.
 * For Phase 1, delegates directly to TaskAgent (L3).
 */
class ChiefArchitectAgent : public BaseAgent {
 public:
  /**
   * @brief Construct a new Chief Architect Agent
   *
   * @param agent_id Unique identifier for this agent
   */
  explicit ChiefArchitectAgent(const std::string& agent_id);

  /**
   * @brief Process incoming message asynchronously
   *
   * FIX C3: Changed to async (returns Task<Response>)
   *
   * @param msg Message to process
   * @return concurrency::Task<core::Response> Async task with response
   */
  concurrency::Task<core::Response> processMessage(const core::KeystoneMessage& msg) override;

  /**
   * @brief Send a command to a task agent and wait for response
   *
   * FIX C3: Changed to async (returns Task<Response>)
   *
   * @param command Command string to execute
   * @param task_agent_id Target task agent ID
   * @return concurrency::Task<core::Response> Async task with response
   */
  concurrency::Task<core::Response> sendCommand(const std::string& command,
                                                 const std::string& task_agent_id);
};

}  // namespace agents
}  // namespace keystone
