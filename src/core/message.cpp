#include "core/message.hpp"
#include <sstream>
#include <iomanip>
#include <random>

namespace keystone {
namespace core {

namespace {
    // Simple UUID generation (not cryptographically secure, but sufficient for Phase 1)
    // Thread-safe: uses thread_local to avoid data races across threads
    std::string generate_uuid() {
        thread_local std::random_device rd;
        thread_local std::mt19937 gen(rd());
        thread_local std::uniform_int_distribution<> dis(0, 15);
        static const char* hex = "0123456789abcdef";

        std::stringstream ss;
        for (int i = 0; i < 32; ++i) {
            if (i == 8 || i == 12 || i == 16 || i == 20) {
                ss << '-';
            }
            ss << hex[dis(gen)];
        }
        return ss.str();
    }
}

KeystoneMessage KeystoneMessage::create(
    const std::string& sender,
    const std::string& receiver,
    const std::string& cmd,
    const std::optional<std::string>& data
) {
    KeystoneMessage msg;
    msg.msg_id = generate_uuid();
    msg.sender_id = sender;
    msg.receiver_id = receiver;
    msg.command = cmd;
    msg.payload = data;
    msg.timestamp = std::chrono::system_clock::now();
    return msg;
}

Response Response::createSuccess(
    const KeystoneMessage& original_msg,
    const std::string& sender,
    const std::string& result_data
) {
    Response resp;
    resp.msg_id = original_msg.msg_id;
    resp.sender_id = sender;
    resp.receiver_id = original_msg.sender_id;
    resp.status = Status::Success;
    resp.result = result_data;
    resp.timestamp = std::chrono::system_clock::now();
    return resp;
}

Response Response::createError(
    const KeystoneMessage& original_msg,
    const std::string& sender,
    const std::string& error_msg
) {
    Response resp;
    resp.msg_id = original_msg.msg_id;
    resp.sender_id = sender;
    resp.receiver_id = original_msg.sender_id;
    resp.status = Status::Error;
    resp.result = error_msg;
    resp.timestamp = std::chrono::system_clock::now();
    return resp;
}

} // namespace core
} // namespace keystone
