#pragma once

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

#include "base_agent.hpp"

namespace keystone {
namespace agents {

/**
 * @brief Level 3 Task Agent
 *
 * Executes concrete tasks including bash commands.
 * Returns results to the commanding agent.
 */
class TaskAgent : public BaseAgent {
 public:
  /**
   * @brief Construct a new Task Agent
   *
   * @param agent_id Unique identifier for this agent
   */
  explicit TaskAgent(const std::string& agent_id);

  /**
   * @brief Process incoming command messages asynchronously
   *
   * FIX C3: Changed to async (returns Task<Response>)
   *
   * @param msg Message containing command to execute
   * @return concurrency::Task<core::Response> Async task with execution result
   */
  concurrency::Task<core::Response> processMessage(const core::KeystoneMessage& msg) override;

  /**
   * @brief Get command execution history
   *
   * @return const std::vector<std::pair<std::string, std::string>>& History of
   * (command, result) pairs
   */
  const std::vector<std::pair<std::string, std::string>>& getCommandHistory()
      const {
    return command_log_;
  }

 private:
  /**
   * @brief RAII wrapper for popen/pclose
   */
  struct PipeDeleter {
    void operator()(FILE* pipe) const {
      if (pipe) {
        pclose(pipe);
      }
    }
  };
  using PipeHandle = std::unique_ptr<FILE, PipeDeleter>;

  /**
   * @brief Validate command for security (FIX P1-03: Command injection prevention)
   *
   * @param command Command to validate
   * @throws std::runtime_error if command contains unsafe characters or is not whitelisted
   */
  void validateCommand(const std::string& command);

  /**
   * @brief Execute a bash command with security validation
   *
   * FIX P1-03: Now validates command before execution to prevent injection attacks.
   *
   * @param command The bash command to execute
   * @return std::string The stdout output
   * @throws std::runtime_error if command fails or is invalid
   */
  std::string executeBash(const std::string& command);

  std::vector<std::pair<std::string, std::string>> command_log_;
};

}  // namespace agents
}  // namespace keystone
