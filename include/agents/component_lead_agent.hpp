#pragma once

#include "base_agent.hpp"
#include <vector>
#include <map>
#include <string>

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
class ComponentLeadAgent : public BaseAgent {
public:
    /**
     * @brief Agent state for tracking workflow
     */
    enum class State {
        IDLE,                    // No active work
        PLANNING,                // Decomposing component goal into modules
        WAITING_FOR_MODULES,     // Modules delegated, waiting for results
        AGGREGATING,             // Combining results from all modules
        ERROR                    // Error state
    };

    /**
     * @brief Construct a new Component Lead Agent
     *
     * @param agent_id Unique identifier for this agent
     */
    explicit ComponentLeadAgent(const std::string& agent_id);

    /**
     * @brief Process incoming message (component goals or module results)
     *
     * @param msg Message to process
     * @return core::Response Response to the message
     */
    core::Response processMessage(const core::KeystoneMessage& msg) override;

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

    // State management
    State current_state_{State::IDLE};
    std::vector<std::string> execution_trace_;

    // Module coordination
    std::vector<std::string> available_module_leads_;
    std::map<std::string, std::string> pending_modules_;  // module_id → module_lead_id
    std::vector<std::string> module_results_;             // Collected module results
    std::string current_component_goal_;
    std::string requester_id_;  // Who sent the component goal

    // Module tracking
    int expected_module_results_{0};
    int received_module_results_{0};
};

} // namespace agents
} // namespace keystone
