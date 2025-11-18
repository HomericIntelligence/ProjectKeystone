#pragma once

#include "core/message.hpp"
#include "concurrentqueue.h"
#include <string>
#include <optional>
#include <atomic>

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

    // FIX: Anti-starvation counters for weighted round-robin
    // After N high-priority messages, force-check lower priorities
    std::atomic<uint64_t> high_priority_processed_{0};
    static constexpr uint64_t HIGH_PRIORITY_QUOTA = 10;  ///< Process max 10 HIGH before checking NORMAL/LOW
};

} // namespace agents
} // namespace keystone
