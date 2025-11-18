#include "agents/async_base_agent.hpp"
#include "concurrency/work_stealing_scheduler.hpp"
#include <stdexcept>

namespace keystone {
namespace agents {

AsyncBaseAgent::AsyncBaseAgent(const std::string& agent_id)
    : AgentBase(agent_id) {
}

void AsyncBaseAgent::receiveMessage(const core::KeystoneMessage& msg) {
    // Call base class to add to inbox
    AgentBase::receiveMessage(msg);

    // If scheduler is set, automatically process the message asynchronously
    if (scheduler_) {
        auto msg_copy = msg;  // Copy for lambda capture
        scheduler_->submit([this, msg_copy]() {
            // Process message asynchronously
            auto task = this->processMessage(msg_copy);
            auto handle = task.get_handle();

            // Resume the coroutine until completion
            // Coroutines may suspend multiple times (e.g., nested co_await)
            while (!handle.done()) {
                handle.resume();
            }
        });
    }
}

void AsyncBaseAgent::setScheduler(concurrency::WorkStealingScheduler* scheduler) {
    scheduler_ = scheduler;
}

void AsyncBaseAgent::processPendingMessages() {
    if (!scheduler_) {
        throw std::runtime_error("Scheduler not set for async agent: " + agent_id_);
    }

    // Get all pending messages using inherited getMessage()
    std::vector<core::KeystoneMessage> messages;
    while (auto msg = getMessage()) {
        messages.push_back(*msg);
    }

    // Submit each message for async processing
    for (const auto& msg : messages) {
        scheduler_->submit([this, msg]() {
            auto task = this->processMessage(msg);
            auto handle = task.get_handle();

            // Resume until completion
            while (!handle.done()) {
                handle.resume();
            }
        });
    }
}

} // namespace agents
} // namespace keystone
