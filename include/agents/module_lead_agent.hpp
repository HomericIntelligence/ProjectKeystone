#pragma once

#include "base_agent.hpp"
#include <vector>
#include <map>
#include <string>

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
class ModuleLeadAgent : public BaseAgent {
public:
    /**
     * @brief Agent state for tracking workflow
     */
    enum class State {
        IDLE,                  // No active work
        PLANNING,              // Decomposing module goal into tasks
        WAITING_FOR_TASKS,     // Tasks delegated, waiting for results
        SYNTHESIZING,          // Combining results from all tasks
        ERROR                  // Error state
    };

    /**
     * @brief Construct a new Module Lead Agent
     *
     * @param agent_id Unique identifier for this agent
     */
    explicit ModuleLeadAgent(const std::string& agent_id);

    /**
     * @brief Process incoming message (module goals or task results)
     *
     * @param msg Message to process
     * @return core::Response Response to the message
     */
    core::Response processMessage(const core::KeystoneMessage& msg) override;

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
};

} // namespace agents
} // namespace keystone
