#pragma once

#include "async_base_agent.hpp"
#include "async_component_lead_agent.hpp"
#include "concurrency/task.hpp"
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace keystone {
namespace agents {

/**
 * @brief Component execution result
 */
struct ComponentResult {
    std::string component_name;
    bool success{false};
    int modules_completed{0};
    int tasks_completed{0};
    std::string error_message;
};

/**
 * @brief Level 0 Async Chief Architect Agent
 *
 * Async version of ChiefArchitectAgent using coroutines.
 * Strategic orchestrator that delegates tasks to lower-level agents asynchronously.
 *
 * **Phase 4 Enhancement**: Supports multi-component coordination with dependency resolution.
 */
class AsyncChiefArchitectAgent : public AsyncBaseAgent {
public:
    /**
     * @brief Construct a new Async Chief Architect Agent
     *
     * @param agent_id Unique identifier for this agent
     */
    explicit AsyncChiefArchitectAgent(const std::string& agent_id);

    /**
     * @brief Process incoming message asynchronously (typically responses from subordinates)
     *
     * @param msg Message to process
     * @return concurrency::Task<core::Response> Coroutine producing response
     */
    concurrency::Task<core::Response> processMessage(const core::KeystoneMessage& msg) override;

    /**
     * @brief Send a command to a subordinate agent asynchronously
     *
     * This method sends a command and returns immediately. The actual processing
     * happens asynchronously on worker threads.
     *
     * @param command Command string to execute
     * @param subordinate_id Target agent ID (TaskAgent, ModuleLead, or ComponentLead)
     */
    void sendCommandAsync(const std::string& command, const std::string& subordinate_id);

    // ========================================================================
    // PHASE 4: Multi-Component Coordination
    // ========================================================================

    /**
     * @brief Register a component lead agent
     *
     * @param component_name Name of the component (e.g., "Core", "Protocol")
     * @param component_lead Pointer to ComponentLeadAgent
     */
    void registerComponent(const std::string& component_name,
                          AsyncComponentLeadAgent* component_lead);

    /**
     * @brief Add dependencies for a component
     *
     * Specifies that a component depends on other components completing first.
     *
     * @param component_name Name of the dependent component
     * @param dependencies List of component names it depends on
     *
     * Example: addComponentDependency("Agents", {"Core", "Protocol"});
     */
    void addComponentDependency(const std::string& component_name,
                               const std::vector<std::string>& dependencies);

    /**
     * @brief Get component execution order (topological sort)
     *
     * Returns components in dependency-respecting order.
     *
     * @return std::vector<std::string> Ordered list of component names
     * @throws std::runtime_error if circular dependency detected
     */
    std::vector<std::string> getComponentExecutionOrder() const;

    /**
     * @brief Execute all components in parallel (respecting dependencies)
     *
     * Submits independent components in parallel to work-stealing scheduler.
     *
     * @param goal High-level goal for the system
     * @return concurrency::Task<std::vector<ComponentResult>> Results from all components
     */
    concurrency::Task<std::vector<ComponentResult>> executeAllComponents(const std::string& goal);

    /**
     * @brief Get registered component names
     *
     * @return std::vector<std::string> List of component names
     */
    std::vector<std::string> getComponentNames() const;

    /**
     * @brief Get number of registered components
     *
     * @return size_t Component count
     */
    size_t getComponentCount() const { return component_registry_.size(); }

private:
    /**
     * @brief Topological sort implementation (DFS-based)
     *
     * @param visited Set of visited nodes
     * @param rec_stack Recursion stack for cycle detection
     * @param component Current component being visited
     * @param result Sorted result (reverse order)
     * @return true if no cycle detected
     * @return false if circular dependency found
     */
    bool topologicalSortUtil(std::unordered_set<std::string>& visited,
                            std::unordered_set<std::string>& rec_stack,
                            const std::string& component,
                            std::vector<std::string>& result) const;

    // Track pending commands for response correlation
    std::map<std::string, std::string> pending_commands_;  // msg_id → command
    std::mutex pending_mutex_;

    // Phase 4: Component management
    std::unordered_map<std::string, AsyncComponentLeadAgent*> component_registry_;
    std::unordered_map<std::string, std::vector<std::string>> component_dependencies_;
    mutable std::mutex component_mutex_;
};

} // namespace agents
} // namespace keystone
