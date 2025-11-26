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
 * @brief Level 1 Component Lead Agent
 *
 * Coordinates multiple ModuleLeadAgents to accomplish component-level goals.
 * Responsibilities:
 * - Decompose component goals into module goals
 * - Distribute module goals to available ModuleLeadAgents
 * - Collect and aggregate results from ModuleLeadAgents
 * - Report aggregated results to ChiefArchitect
 *
 * State Machine: IDLE → PLANNING → WAITING_FOR_MODULES → AGGREGATING → IDLE
 */
class ComponentLeadAgent : public AsyncAgent {
 public:
  /**
   * @brief Agent state for tracking workflow
   */
  enum class State {
    IDLE,                 // No active work
    PLANNING,             // Decomposing component goal into modules
    WAITING_FOR_MODULES,  // Modules delegated, waiting for results
    AGGREGATING,          // Combining results from all modules
    ERROR                 // Error state
  };

  /**
   * @brief Construct a new Component Lead Agent
   *
   * @param agent_id Unique identifier for this agent
   */
  explicit ComponentLeadAgent(const std::string& agent_id);

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
   * @brief Configure available ModuleLeadAgents for delegation
   *
   * @param module_lead_ids List of ModuleLead IDs available for work
   */
  void setAvailableModuleLeads(const std::vector<std::string>& module_lead_ids);

  /**
   * @brief Process a result from a ModuleLeadAgent
   *
   * @param result_msg Message containing module result
   */
  void processModuleResult(const core::KeystoneMessage& result_msg);

  /**
   * @brief Aggregate results from all completed modules
   *
   * @return std::string Aggregated component-level result
   */
  std::string synthesizeComponentResult();

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
   * @param agent_type Agent type (default: "ComponentLeadAgent")
   * @param level Agent level (default: 1)
   */
  void initializeGrpc(const std::string& coordinator_address,
                      const std::string& registry_address,
                      const std::string& agent_type = "ComponentLeadAgent",
                      int level = 1);

  /**
   * @brief Process incoming YAML task specification
   *
   * @param yaml_spec YAML component specification
   */
  void processYamlComponent(const std::string& yaml_spec);

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
   * @brief Decompose component goal into module goals
   *
   * @param component_goal High-level component goal
   * @return std::vector<std::string> List of module goals
   */
  std::vector<std::string> decomposeModules(const std::string& component_goal);

  /**
   * @brief Delegate module goals to available ModuleLeadAgents
   *
   * @param module_goals List of module goals to delegate
   */
  void delegateModules(const std::vector<std::string>& module_goals);

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
  std::vector<std::string> available_module_leads_;

#ifdef ENABLE_GRPC
  // Result aggregator (component-specific, not in template)
  std::unique_ptr<network::ResultAggregator> result_aggregator_;
#endif
};

}  // namespace agents
}  // namespace keystone
