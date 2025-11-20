#pragma once

#include "core/message.hpp"
#include "core/config.hpp"  // FIX m3: Centralized configuration
#include "concurrentqueue.h"
#include <string>
#include <optional>
#include <atomic>
#include <chrono>  // FIX M2: For time-based priority fairness

namespace keystone {

// Forward declaration
namespace core {
    class MessageBus;
}

namespace agents {

/**
 * @brief Common base class for all agents (sync and async)
 *
 * Provides inbox management and message routing functionality.
 * Subclasses implement either sync or async message processing.
 *
 * Phase C Enhancement: Uses lock-free concurrent queue for inbox
 * to eliminate contention under high message load.
 */
class AgentBase {
public:
    /**
     * @brief Construct a new Agent Base
     *
     * @param agent_id Unique identifier for this agent
     */
    explicit AgentBase(const std::string& agent_id);

    virtual ~AgentBase() = default;

    /**
     * @brief Send a message via the message bus
     *
     * @param msg Message to send
     * @throws std::runtime_error if message bus not set
     */
    void sendMessage(const core::KeystoneMessage& msg);

    /**
     * @brief Receive a message into the inbox (called by MessageBus)
     *
     * Lock-free operation - no mutex contention.
     *
     * @param msg Message to receive
     */
    virtual void receiveMessage(const core::KeystoneMessage& msg);

    /**
     * @brief Get the next message from inbox
     *
     * Lock-free operation - no mutex contention.
     *
     * @return std::optional<core::KeystoneMessage> Next message if available
     */
    std::optional<core::KeystoneMessage> getMessage();

    /**
     * @brief Get the agent ID
     *
     * @return const std::string& Agent identifier
     */
    const std::string& getAgentId() const { return agent_id_; }

    /**
     * @brief Set the message bus for this agent
     *
     * @param bus Pointer to message bus (must outlive agent)
     */
    void setMessageBus(core::MessageBus* bus);

protected:
    /**
     * @brief Update queue depth metrics
     *
     * Calculates total messages across all priority queues and reports to Metrics.
     * Called after each getMessage() operation.
     */
    void updateQueueDepthMetrics();
    std::string agent_id_;
    core::MessageBus* message_bus_{nullptr};

    // Phase C: Priority-based lock-free inboxes
    // Messages are routed to queues by priority and processed HIGH -> NORMAL -> LOW
    moodycamel::ConcurrentQueue<core::KeystoneMessage> high_priority_inbox_;
    moodycamel::ConcurrentQueue<core::KeystoneMessage> normal_priority_inbox_;
    moodycamel::ConcurrentQueue<core::KeystoneMessage> low_priority_inbox_;

    // FIX M2: Anti-starvation using time-based fairness (not count-based)
    // Force-check lower priorities every N milliseconds to prevent starvation
    // under sustained HIGH priority load
    std::chrono::steady_clock::time_point last_low_priority_check_;

    // FIX M1: Backpressure - Queue size limits to prevent memory exhaustion
    std::atomic<bool> backpressure_applied_{false};  ///< Flag when backpressure is active
};

} // namespace agents
} // namespace keystone
