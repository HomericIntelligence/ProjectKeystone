#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "agents/async_agent.hpp"
#include "agents/coordination_state.hpp"

#ifdef ENABLE_GRPC
#include "network/result_aggregator.hpp"
#include "network/yaml_parser.hpp"
#endif

namespace keystone {
namespace agents {

/**
 * @brief Level 2 Module Lead Agent
 *
 * Coordinates multiple TaskAgents to accomplish module-level goals.
 * Responsibilities:
 * - Decompose module goals into individual tasks
 * - Distribute tasks to available TaskAgents
 * - Collect and synthesize results from TaskAgents
 * - Report synthesized results to ChiefArchitect
 *
 * State Machine: IDLE → PLANNING → WAITING_FOR_TASKS → SYNTHESIZING → IDLE
 */
class ModuleLeadAgent : public AsyncAgent {
 public:
  /**
   * @brief Agent state for tracking workflow
   */
  enum class State {
    IDLE,               // No active work
    PLANNING,           // Decomposing module goal into tasks
    WAITING_FOR_TASKS,  // Tasks delegated, waiting for results
    SYNTHESIZING,       // Combining results from all tasks
    ERROR               // Error state
  };

  /**
   * @brief Construct a new Module Lead Agent
   *
   * @param agent_id Unique identifier for this agent
   */
  explicit ModuleLeadAgent(const std::string& agent_id);

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
   * @brief Configure available TaskAgents for delegation
   *
   * @param task_agent_ids List of TaskAgent IDs available for work
   */
  void setAvailableTaskAgents(const std::vector<std::string>& task_agent_ids);

  /**
   * @brief Process a result from a TaskAgent
   *
   * @param result_msg Message containing task result
   */
  void processTaskResult(const core::KeystoneMessage& result_msg);

  /**
   * @brief Synthesize results from all completed tasks
   *
   * @return std::string Synthesized module-level result
   */
  std::string synthesizeResults();

  /**
   * @brief Get execution trace for testing/debugging
   *
   * @return std::vector<std::string> State transition history (copy for thread safety)
   */
  std::vector<std::string> getExecutionTrace() const {
    return coordination_.getExecutionTrace();
  }

  /**
   * @brief Get current state
   *
   * @return State Current agent state
   */
  State getCurrentState() const { return coordination_.getCurrentState(); }

#ifdef ENABLE_GRPC
  /**
   * @brief Initialize gRPC clients and register with ServiceRegistry
   *
   * @param coordinator_address HMASCoordinator server address
   * @param registry_address ServiceRegistry server address
   * @param agent_type Agent type (default: "ModuleLeadAgent")
   * @param level Agent level (default: 2)
   */
  void initializeGrpc(const std::string& coordinator_address,
                      const std::string& registry_address,
                      const std::string& agent_type = "ModuleLeadAgent",
                      int level = 2);

  /**
   * @brief Process incoming YAML task specification
   *
   * @param yaml_spec YAML module specification
   */
  void processYamlModule(const std::string& yaml_spec);

  /**
   * @brief Start heartbeat thread (sends heartbeat every 1s)
   */
  void startHeartbeat();

  /**
   * @brief Stop heartbeat thread
   */
  void stopHeartbeat();

  /**
   * @brief Shutdown agent and unregister from ServiceRegistry
   */
  void shutdown();
#endif

 private:
  /**
   * @brief Decompose module goal into individual tasks
   *
   * @param module_goal High-level module goal
   * @return std::vector<std::string> List of individual tasks
   */
  std::vector<std::string> decomposeTasks(const std::string& module_goal);

  /**
   * @brief Delegate tasks to available TaskAgents
   *
   * @param tasks List of tasks to delegate
   */
  void delegateTasks(const std::vector<std::string>& tasks);

  /**
   * @brief Convert state enum to string for tracing
   *
   * @param state State to convert
   * @return std::string String representation
   */
  std::string stateToString(State state) const;

  // Coordination state (eliminates ~300 lines of duplication)
  CoordinationState<State, std::string> coordination_;

  // Agent-specific configuration
  std::vector<std::string> available_task_agents_;

#ifdef ENABLE_GRPC
  // Result aggregator (module-specific, not in template)
  std::unique_ptr<network::ResultAggregator> result_aggregator_;
#endif
};

}  // namespace agents
}  // namespace keystone
