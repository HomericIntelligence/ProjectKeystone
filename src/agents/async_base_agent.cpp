#include "agents/async_base_agent.hpp"
#include "concurrency/work_stealing_scheduler.hpp"
#include <stdexcept>

namespace keystone {
namespace agents {

AsyncBaseAgent::AsyncBaseAgent(const std::string& agent_id)
    : AgentBase(agent_id) {
}

void AsyncBaseAgent::receiveMessage(const core::KeystoneMessage& msg) {
    // Phase 5: Check if agent is failed
    if (is_failed_.load()) {
        // Reject message with error response
        std::string error_msg;
        {
            std::lock_guard<std::mutex> lock(failure_mutex_);
            error_msg = failure_reason_.empty() ? "agent failed" : failure_reason_;
        }

        auto error_response = core::Response::createError(msg, agent_id_, error_msg);

        // Send error response back if MessageBus is available
        if (message_bus_) {
            auto response_msg = core::KeystoneMessage::create(
                agent_id_,
                msg.sender_id,
                "response",
                error_response.result
            );
            sendMessage(response_msg);
        }
        return;  // Don't process message
    }

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

// ============================================================================
// PHASE 5: Failure Modes (Chaos Engineering)
// ============================================================================

void AsyncBaseAgent::markAsFailed(const std::string& reason) {
    std::lock_guard<std::mutex> lock(failure_mutex_);
    is_failed_.store(true);
    failure_reason_ = reason;
}

void AsyncBaseAgent::recover() {
    std::lock_guard<std::mutex> lock(failure_mutex_);
    is_failed_.store(false);
    failure_reason_.clear();
}

bool AsyncBaseAgent::isFailed() const {
    return is_failed_.load();
}

std::string AsyncBaseAgent::getFailureReason() const {
    std::lock_guard<std::mutex> lock(failure_mutex_);
    return failure_reason_;
}

void AsyncBaseAgent::setFailureInjector(core::FailureInjector* injector) {
    failure_injector_ = injector;
}

bool AsyncBaseAgent::shouldFail() const {
    if (!failure_injector_) {
        return false;
    }
    return failure_injector_->shouldAgentFail(agent_id_);
}

} // namespace agents
} // namespace keystone
