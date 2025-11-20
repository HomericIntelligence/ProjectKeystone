#pragma once

#include "async_base_agent.hpp"
#include "concurrency/task.hpp"

#include <cstdio>
#include <future>
#include <memory>
#include <utility>
#include <vector>

namespace keystone {
namespace agents {

/**
 * @brief Level 3 Async Task Agent
 *
 * Executes concrete tasks including bash commands asynchronously.
 * Uses coroutines to avoid blocking worker threads during I/O.
 *
 * Phase B: processMessage() returns Task<Response> and can co_await
 * async bash execution.
 */
class AsyncTaskAgent : public AsyncBaseAgent {
 public:
  /**
   * @brief Construct a new Async Task Agent
   *
   * @param agent_id Unique identifier for this agent
   */
  explicit AsyncTaskAgent(const std::string& agent_id);

  /**
   * @brief Process incoming command messages asynchronously
   *
   * @param msg Message containing command to execute
   * @return concurrency::Task<core::Response> Coroutine producing response
   */
  concurrency::Task<core::Response> processMessage(const core::KeystoneMessage& msg) override;

  /**
   * @brief Get command execution history
   *
   * @return const std::vector<std::pair<std::string, std::string>>& History of (command, result)
   * pairs
   */
  const std::vector<std::pair<std::string, std::string>>& getCommandHistory() const {
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
   * @brief Execute a bash command asynchronously
   *
   * This runs the blocking popen operation in the background using
   * std::async, allowing the coroutine to co_await completion without
   * blocking the worker thread.
   *
   * @param command The bash command to execute
   * @return concurrency::Task<std::string> Coroutine that yields stdout output
   * @throws std::runtime_error if command fails
   */
  concurrency::Task<std::string> executeBashAsync(const std::string& command);

  /**
   * @brief Execute a bash command synchronously (helper for executeBashAsync)
   *
   * This is the blocking implementation that runs in std::async.
   *
   * @param command The bash command to execute
   * @return std::string The stdout output
   * @throws std::runtime_error if command fails
   */
  static std::string executeBashSync(const std::string& command);

  std::vector<std::pair<std::string, std::string>> command_log_;
};

}  // namespace agents
}  // namespace keystone
