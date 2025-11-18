#include "agents/agent_base.hpp"
#include "core/message_bus.hpp"
#include <stdexcept>

namespace keystone {
namespace agents {

AgentBase::AgentBase(const std::string& agent_id)
    : agent_id_(agent_id) {
}

void AgentBase::sendMessage(const core::KeystoneMessage& msg) {
    if (!message_bus_) {
        throw std::runtime_error("Message bus not set for agent: " + agent_id_);
    }

    message_bus_->routeMessage(msg);
}

void AgentBase::receiveMessage(const core::KeystoneMessage& msg) {
    std::lock_guard<std::mutex> lock(inbox_mutex_);
    inbox_.push(msg);
}

std::optional<core::KeystoneMessage> AgentBase::getMessage() {
    std::lock_guard<std::mutex> lock(inbox_mutex_);
    if (inbox_.empty()) {
        return std::nullopt;
    }
    auto msg = inbox_.front();
    inbox_.pop();
    return msg;
}

void AgentBase::setMessageBus(core::MessageBus* bus) {
    message_bus_ = bus;
}

} // namespace agents
} // namespace keystone
