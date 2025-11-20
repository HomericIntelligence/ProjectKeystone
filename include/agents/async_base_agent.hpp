#pragma once

#include "agent_base.hpp"
#include "core/message.hpp"
#include "core/failure_injector.hpp"
#include "concurrency/task.hpp"
#include <string>
#include <atomic>
#include <chrono>

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

    /**
     * @brief Destructor - marks agent as destroyed for async safety
     *
     * FIX C2: Sets is_destroyed_ flag to prevent use-after-free in async lambdas
     */
    ~AsyncBaseAgent() override;

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

    // ========================================================================
    // PHASE 5: Failure Modes (Chaos Engineering)
    // ========================================================================

    /**
     * @brief Mark this agent as failed
     *
     * When failed, agent will reject all incoming messages with error responses.
     * Used for chaos engineering and robustness testing.
     *
     * @param reason Reason for failure (e.g., "simulated crash")
     */
    void markAsFailed(const std::string& reason = "agent failed");

    /**
     * @brief Recover this agent from failed state
     *
     * Restores normal operation after failure.
     */
    void recover();

    /**
     * @brief Check if this agent is in failed state
     *
     * @return true if agent is failed
     */
    bool isFailed() const;

    /**
     * @brief Get failure reason
     *
     * @return std::string Failure reason (empty if not failed)
     */
    std::string getFailureReason() const;

    /**
     * @brief Set failure injector for chaos testing
     *
     * The failure injector can probabilistically fail agents or inject timeouts.
     *
     * @param injector Pointer to failure injector (must outlive agent)
     */
    void setFailureInjector(core::FailureInjector* injector);

    /**
     * @brief Check if agent should fail based on failure injector
     *
     * @return true if agent should fail (according to injector)
     */
    bool shouldFail() const;

protected:
    concurrency::WorkStealingScheduler* scheduler_{nullptr};

    // Phase 5: Failure state
    std::atomic<bool> is_failed_{false};
    std::string failure_reason_;
    mutable std::mutex failure_mutex_;
    core::FailureInjector* failure_injector_{nullptr};

    // FIX C2: Lifetime tracking to prevent use-after-free
    // This flag is checked in async lambdas to ensure agent is still alive
    // IMPORTANT: Agents must outlive the scheduler, or scheduler must be
    // drained before agent destruction. See shutdown sequence documentation.
    std::shared_ptr<std::atomic<bool>> is_destroyed_;
};

} // namespace agents
} // namespace keystone
