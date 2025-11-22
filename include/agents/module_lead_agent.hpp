#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "agents/async_agent.hpp"

#ifdef ENABLE_GRPC
#include "network/grpc_client.hpp"
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
   * @return const std::vector<std::string>& State transition history
   */
  const std::vector<std::string>& getExecutionTrace() const {
    return execution_trace_;
  }

  /**
   * @brief Get current state
   *
   * @return State Current agent state
   */
  State getCurrentState() const { return current_state_; }

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
   * @brief Transition to a new state
   *
   * @param new_state State to transition to
   */
  void transitionTo(State new_state);

  /**
   * @brief Convert state enum to string for tracing
   *
   * @param state State to convert
   * @return std::string String representation
   */
  std::string stateToString(State state) const;

#ifdef ENABLE_GRPC
  /**
   * @brief Heartbeat loop (runs in separate thread)
   */
  void heartbeatLoop();

  /**
   * @brief Query ServiceRegistry for available TaskAgents
   *
   * @return std::vector<std::string> List of available TaskAgent IDs
   */
  std::vector<std::string> queryAvailableTaskAgents();
#endif

  // State management
  State current_state_{State::IDLE};
  std::vector<std::string> execution_trace_;

  // Task coordination
  std::vector<std::string> available_task_agents_;
  std::map<std::string, std::string> pending_tasks_;  // task_id → agent_id
  std::vector<std::string> task_results_;             // Collected results
  std::string current_module_goal_;
  std::string requester_id_;  // Who sent the module goal

  // Task tracking
  int expected_results_{0};
  int received_results_{0};

#ifdef ENABLE_GRPC
  // gRPC clients
  std::unique_ptr<network::HMASCoordinatorClient> coordinator_client_;
  std::unique_ptr<network::ServiceRegistryClient> registry_client_;

  // Result aggregator
  std::unique_ptr<network::ResultAggregator> result_aggregator_;

  // Heartbeat thread
  std::thread heartbeat_thread_;
  std::atomic<bool> heartbeat_running_{false};

  // Agent metadata
  std::string agent_type_{"ModuleLeadAgent"};
  int agent_level_{2};

  // Current task being processed
  std::string current_task_id_;
#endif
};

}  // namespace agents
}  // namespace keystone
