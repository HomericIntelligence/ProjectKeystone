#pragma once

#include "message.hpp"
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <vector>

namespace keystone {

// Forward declaration
namespace agents {
    class BaseAgent;
}

namespace core {

/**
 * @brief Central message routing hub for agent communication
 *
 * MessageBus decouples agents from each other, enabling:
 * - Dynamic agent registration/discovery
 * - Automatic message routing by agent ID
 * - Future evolution to async/distributed messaging
 */
class MessageBus {
public:
    MessageBus() = default;
    ~MessageBus() = default;

    // Non-copyable, non-movable
    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;
    MessageBus(MessageBus&&) = delete;
    MessageBus& operator=(MessageBus&&) = delete;

    /**
     * @brief Register an agent with the bus
     *
     * @param agent_id Unique identifier for the agent
     * @param agent Pointer to the agent (must outlive registration)
     * @throws std::runtime_error if agent_id already registered
     */
    void registerAgent(const std::string& agent_id, agents::BaseAgent* agent);

    /**
     * @brief Unregister an agent from the bus
     *
     * @param agent_id Agent identifier to unregister
     */
    void unregisterAgent(const std::string& agent_id);

    /**
     * @brief Route a message to the appropriate agent
     *
     * @param msg Message to route (uses msg.receiver_id for routing)
     * @return true if message was delivered, false if receiver not found
     */
    bool routeMessage(const KeystoneMessage& msg);

    /**
     * @brief Check if an agent is registered
     *
     * @param agent_id Agent identifier to check
     * @return true if agent is registered
     */
    bool hasAgent(const std::string& agent_id) const;

    /**
     * @brief Get list of all registered agent IDs
     *
     * @return std::vector<std::string> List of agent IDs
     */
    std::vector<std::string> listAgents() const;

private:
    mutable std::mutex registry_mutex_;
    std::unordered_map<std::string, agents::BaseAgent*> agents_;
};

} // namespace core
} // namespace keystone
