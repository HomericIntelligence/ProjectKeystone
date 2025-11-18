#pragma once

#include "async_base_agent.hpp"
#include "concurrency/task.hpp"
#include <vector>
#include <map>
#include <string>
#include <mutex>

namespace keystone {
namespace agents {

/**
 * @brief Level 1 Async Component Lead Agent
 *
 * Async version of ComponentLeadAgent using coroutines.
 * Coordinates multiple async ModuleLeadAgents to accomplish component-level goals.
 *
 * Responsibilities:
 * - Decompose component goals into module goals
 * - Distribute module goals to available ModuleLeadAgents asynchronously
 * - Collect and aggregate results from ModuleLeadAgents
 * - Report aggregated results to ChiefArchitect
 *
 * State Machine: IDLE → PLANNING → WAITING_FOR_MODULES → AGGREGATING → IDLE
 */
class AsyncComponentLeadAgent : public AsyncBaseAgent {
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
     * @brief Construct a new Async Component Lead Agent
     *
     * @param agent_id Unique identifier for this agent
     */
    explicit AsyncComponentLeadAgent(const std::string& agent_id);

    /**
     * @brief Process incoming message asynchronously (component goals or module results)
     *
     * @param msg Message to process
     * @return concurrency::Task<core::Response> Coroutine producing response
     */
    concurrency::Task<core::Response> processMessage(const core::KeystoneMessage& msg) override;

    /**
     * @brief Configure available ModuleLeadAgents for delegation
     *
     * @param module_lead_ids List of ModuleLead IDs available for work
     */
    void setAvailableModuleLeads(const std::vector<std::string>& module_lead_ids);

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
    State getCurrentState() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return current_state_;
    }

private:
    /**
     * @brief Process a result from a ModuleLeadAgent
     *
     * @param result_msg Message containing module result
     * @return concurrency::Task<void> Coroutine for async processing
     */
    concurrency::Task<void> processModuleResult(const core::KeystoneMessage& result_msg);

    /**
     * @brief Decompose component goal into module goals
     *
     * @param component_goal High-level component goal
     * @return std::vector<std::string> List of module goals
     */
    std::vector<std::string> decomposeModules(const std::string& component_goal);

    /**
     * @brief Delegate module goals to available ModuleLeadAgents asynchronously
     *
     * @param module_goals List of module goals to delegate
     */
    void delegateModules(const std::vector<std::string>& module_goals);

    /**
     * @brief Aggregate results and send back to requester
     *
     * Automatically called when all module results are received.
     */
    void aggregateAndRespond();

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

    // State management (thread-safe)
    mutable std::mutex state_mutex_;
    State current_state_{State::IDLE};
    std::vector<std::string> execution_trace_;

    // Module coordination (thread-safe)
    std::mutex module_mutex_;
    std::vector<std::string> available_module_leads_;
    std::map<std::string, std::string> pending_modules_;  // msg_id → module_lead_id
    std::vector<std::string> module_results_;             // Collected module results
    std::string current_component_goal_;
    std::string requester_id_;  // Who sent the component goal
    std::string current_request_msg_id_;  // For response correlation

    // Module tracking
    int expected_module_results_{0};
    int received_module_results_{0};
};

} // namespace agents
} // namespace keystone
