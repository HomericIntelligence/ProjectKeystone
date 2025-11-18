#pragma once

#include "async_base_agent.hpp"
#include "concurrency/task.hpp"
#include <map>
#include <string>

namespace keystone {
namespace agents {

/**
 * @brief Level 0 Async Chief Architect Agent
 *
 * Async version of ChiefArchitectAgent using coroutines.
 * Strategic orchestrator that delegates tasks to lower-level agents asynchronously.
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

private:
    // Track pending commands for response correlation
    std::map<std::string, std::string> pending_commands_;  // msg_id → command
    std::mutex pending_mutex_;
};

} // namespace agents
} // namespace keystone
