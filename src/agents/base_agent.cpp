#include "agents/base_agent.hpp"

namespace keystone {
namespace agents {

BaseAgent::BaseAgent(const std::string& agent_id)
    : agent_id_(agent_id) {
}

void BaseAgent::sendMessage(const core::KeystoneMessage& msg, BaseAgent* target) {
    if (target) {
        target->receiveMessage(msg);
    }
}

void BaseAgent::receiveMessage(const core::KeystoneMessage& msg) {
    std::lock_guard<std::mutex> lock(inbox_mutex_);
    inbox_.push(msg);
}

std::optional<core::KeystoneMessage> BaseAgent::getMessage() {
    std::lock_guard<std::mutex> lock(inbox_mutex_);
    if (inbox_.empty()) {
        return std::nullopt;
    }
    auto msg = inbox_.front();
    inbox_.pop();
    return msg;
}

void BaseAgent::storeResponse(const core::Response& resp) {
    std::lock_guard<std::mutex> lock(response_mutex_);
    stored_response_ = resp;
}

std::optional<core::Response> BaseAgent::getStoredResponse() {
    std::lock_guard<std::mutex> lock(response_mutex_);
    return stored_response_;
}

} // namespace agents
} // namespace keystone
