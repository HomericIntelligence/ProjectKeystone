#pragma once

#include "core/message.hpp"
#include <string>
#include <queue>
#include <mutex>
#include <optional>
#include <memory>

namespace keystone {

// Forward declaration
namespace core {
    class MessageBus;
}

namespace agents {

/**
 * @brief Base class for all agents in the hierarchy
 *
 * Provides common functionality for message handling and agent lifecycle.
 * For Phase 1, uses simple synchronous message processing.
 */
class BaseAgent {
public:
    /**
     * @brief Construct a new Base Agent
     *
     * @param agent_id Unique identifier for this agent
     */
    explicit BaseAgent(const std::string& agent_id);

    virtual ~BaseAgent() = default;

    /**
     * @brief Process an incoming message
     *
     * @param msg The message to process
     * @return core::Response Response to the message
     */
    virtual core::Response processMessage(const core::KeystoneMessage& msg) = 0;

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
     * @param msg Message to receive
     */
    void receiveMessage(const core::KeystoneMessage& msg);

    /**
     * @brief Get the next message from inbox
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
    std::string agent_id_;
    core::MessageBus* message_bus_{nullptr};

private:
    std::queue<core::KeystoneMessage> inbox_;
    std::mutex inbox_mutex_;
};

} // namespace agents
} // namespace keystone
