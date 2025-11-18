#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <map>

namespace keystone {
namespace core {

/**
 * @brief Keystone Interchange Message (KIM) protocol
 *
 * Simple message structure for Phase 1 agent communication.
 * Messages are passed between agents to delegate tasks and return results.
 */
struct KeystoneMessage {
    std::string msg_id;        ///< Unique message identifier
    std::string sender_id;     ///< ID of the sending agent
    std::string receiver_id;   ///< ID of the receiving agent
    std::string command;       ///< Command string to execute
    std::optional<std::string> payload;  ///< Optional payload data
    std::chrono::system_clock::time_point timestamp;  ///< Message timestamp

    /**
     * @brief Create a new message with generated ID
     *
     * @param sender Sender agent ID
     * @param receiver Receiver agent ID
     * @param cmd Command string
     * @param data Optional payload data
     * @return KeystoneMessage New message with auto-generated ID
     */
    static KeystoneMessage create(
        const std::string& sender,
        const std::string& receiver,
        const std::string& cmd,
        const std::optional<std::string>& data = std::nullopt
    );
};

/**
 * @brief Response to a KeystoneMessage
 */
struct Response {
    std::string msg_id;        ///< ID of the original message
    std::string sender_id;     ///< ID of the responding agent
    std::string receiver_id;   ///< ID of the original sender
    enum class Status {
        Success,
        Error
    } status;
    std::string result;        ///< Result data or error message
    std::chrono::system_clock::time_point timestamp;

    /**
     * @brief Create a success response
     *
     * @param original_msg The message being responded to
     * @param sender The responding agent ID
     * @param result_data The result data
     * @return Response Success response
     */
    static Response createSuccess(
        const KeystoneMessage& original_msg,
        const std::string& sender,
        const std::string& result_data
    );

    /**
     * @brief Create an error response
     *
     * @param original_msg The message being responded to
     * @param sender The responding agent ID
     * @param error_msg The error message
     * @return Response Error response
     */
    static Response createError(
        const KeystoneMessage& original_msg,
        const std::string& sender,
        const std::string& error_msg
    );
};

} // namespace core
} // namespace keystone
