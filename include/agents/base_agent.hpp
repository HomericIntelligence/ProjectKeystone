#pragma once

#include <string>

#include "agent_base.hpp"
#include "core/message.hpp"

namespace keystone {
namespace agents {

/**
 * @brief Synchronous agent base class
 *
 * Provides synchronous message processing via processMessage().
 * For Phase 1-3 agents.
 */
class BaseAgent : public AgentBase {
 public:
  /**
   * @brief Construct a new Base Agent
   *
   * @param agent_id Unique identifier for this agent
   */
  explicit BaseAgent(const std::string& agent_id);

  ~BaseAgent() override = default;

  /**
   * @brief Process an incoming message synchronously
   *
   * @param msg The message to process
   * @return core::Response Response to the message
   */
  virtual core::Response processMessage(const core::KeystoneMessage& msg) = 0;
};

}  // namespace agents
}  // namespace keystone
