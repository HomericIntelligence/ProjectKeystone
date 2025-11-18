#pragma once

#include "agent_base.hpp"
#include "core/message.hpp"
#include "concurrency/task.hpp"
#include <string>

namespace keystone {

namespace concurrency {
    class WorkStealingScheduler;
}

namespace agents {

/**
 * @brief Async base class for all agents using coroutines
 *
 * Phase B: Agents with async processMessage() that returns Task<Response>.
 * This enables agents to co_await async operations without blocking worker threads.
 */
class AsyncBaseAgent : public AgentBase {
public:
    /**
     * @brief Construct a new Async Base Agent
     *
     * @param agent_id Unique identifier for this agent
     */
    explicit AsyncBaseAgent(const std::string& agent_id);

    ~AsyncBaseAgent() override = default;

    /**
     * @brief Process an incoming message asynchronously
     *
     * This coroutine can co_await async operations (I/O, sub-tasks, etc.)
     * without blocking worker threads.
     *
     * @param msg The message to process
     * @return concurrency::Task<core::Response> Coroutine that produces response
     */
    virtual concurrency::Task<core::Response> processMessage(const core::KeystoneMessage& msg) = 0;

    /**
     * @brief Receive a message into the inbox (called by MessageBus)
     *
     * This overrides AgentBase to automatically submit messages for async processing
     * when a scheduler is set.
     *
     * @param msg Message to receive
     */
    void receiveMessage(const core::KeystoneMessage& msg) override;

    /**
     * @brief Set the scheduler for async operations
     *
     * The scheduler is used to execute async work items (I/O, sub-tasks, etc.)
     *
     * @param scheduler Pointer to scheduler (must outlive agent)
     */
    void setScheduler(concurrency::WorkStealingScheduler* scheduler);

    /**
     * @brief Process all pending messages in the inbox
     *
     * This submits all queued messages to the scheduler for async processing.
     * Returns immediately - processing happens asynchronously.
     */
    void processPendingMessages();

protected:
    concurrency::WorkStealingScheduler* scheduler_{nullptr};
};

} // namespace agents
} // namespace keystone
