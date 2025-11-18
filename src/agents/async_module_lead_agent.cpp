#include "agents/async_module_lead_agent.hpp"
#include <sstream>
#include <regex>

namespace keystone {
namespace agents {

AsyncModuleLeadAgent::AsyncModuleLeadAgent(const std::string& agent_id)
    : AsyncBaseAgent(agent_id) {
    transitionTo(State::IDLE);
}

concurrency::Task<core::Response> AsyncModuleLeadAgent::processMessage(const core::KeystoneMessage& msg) {
    // Check if this is a task result (from TaskAgent) or a module goal (from ChiefArchitect)
    if (msg.command == "response") {
        // This is a task result - process asynchronously
        co_await processTaskResult(msg);
        auto response = core::Response::createSuccess(msg, agent_id_, "Task result processed");
        co_return response;
    }

    // This is a module goal from ChiefArchitect/ComponentLead
    transitionTo(State::PLANNING);

    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        current_module_goal_ = msg.command;
        requester_id_ = msg.sender_id;
        current_request_msg_id_ = msg.msg_id;
    }

    // Decompose module goal into tasks
    auto tasks = decomposeTasks(msg.command);

    if (tasks.empty()) {
        transitionTo(State::ERROR);
        auto response = core::Response::createError(msg, agent_id_, "Failed to decompose module goal");
        co_return response;
    }

    // Delegate tasks to TaskAgents
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        expected_results_ = static_cast<int>(tasks.size());
        received_results_ = 0;
        task_results_.clear();
    }

    transitionTo(State::WAITING_FOR_TASKS);
    delegateTasks(tasks);

    auto response = core::Response::createSuccess(msg, agent_id_,
        "Module goal decomposed into " + std::to_string(tasks.size()) + " tasks");
    co_return response;
}

void AsyncModuleLeadAgent::setAvailableTaskAgents(const std::vector<std::string>& task_agent_ids) {
    std::lock_guard<std::mutex> lock(task_mutex_);
    available_task_agents_ = task_agent_ids;
}

concurrency::Task<void> AsyncModuleLeadAgent::processTaskResult(const core::KeystoneMessage& result_msg) {
    if (!result_msg.payload) {
        co_return;
    }

    bool all_received = false;
    {
        std::lock_guard<std::mutex> lock(task_mutex_);

        // Store the result
        task_results_.push_back(*result_msg.payload);
        received_results_++;

        // Check if we've received all results
        if (received_results_ >= expected_results_) {
            transitionTo(State::SYNTHESIZING);
            all_received = true;
        }
    }

    // If all results received, synthesize and respond
    if (all_received) {
        synthesizeAndRespond();
    }

    co_return;
}

std::vector<std::string> AsyncModuleLeadAgent::decomposeTasks(const std::string& module_goal) {
    std::vector<std::string> tasks;

    // Parse goal like "Calculate sum of: 10 + 20 + 30"
    // or "Calculate: 100 + 200"

    // Extract numbers using regex
    std::regex number_regex(R"(\d+)");
    auto numbers_begin = std::sregex_iterator(module_goal.begin(), module_goal.end(), number_regex);
    auto numbers_end = std::sregex_iterator();

    // Create a task for each number (echo the number)
    for (std::sregex_iterator i = numbers_begin; i != numbers_end; ++i) {
        std::string number = i->str();
        std::string task = "echo " + number;
        tasks.push_back(task);
    }

    return tasks;
}

void AsyncModuleLeadAgent::delegateTasks(const std::vector<std::string>& tasks) {
    std::lock_guard<std::mutex> lock(task_mutex_);

    // Assign tasks to available TaskAgents in round-robin fashion
    for (size_t i = 0; i < tasks.size(); ++i) {
        if (available_task_agents_.empty()) {
            throw std::runtime_error("No TaskAgents available for delegation");
        }

        // Round-robin assignment
        size_t agent_index = i % available_task_agents_.size();
        const std::string& task_agent_id = available_task_agents_[agent_index];

        // Create and send task message
        auto task_msg = core::KeystoneMessage::create(
            agent_id_,
            task_agent_id,
            tasks[i]
        );

        // Track pending task
        pending_tasks_[task_msg.msg_id] = task_agent_id;

        // Send via MessageBus (async routing)
        sendMessage(task_msg);
    }
}

void AsyncModuleLeadAgent::synthesizeAndRespond() {
    std::string synthesis;
    std::string requester;
    std::string request_msg_id;

    {
        std::lock_guard<std::mutex> lock(task_mutex_);

        // Parse each result as a number and sum them
        int total = 0;
        for (const auto& result : task_results_) {
            try {
                int value = std::stoi(result);
                total += value;
            } catch (const std::exception&) {
                // Skip non-numeric results
            }
        }

        // Create synthesis message
        std::stringstream ss;
        ss << "Module result: Sum = " << total;
        synthesis = ss.str();

        requester = requester_id_;
        request_msg_id = current_request_msg_id_;
    }

    transitionTo(State::IDLE);

    // Send synthesized result back to requester (ChiefArchitect/ComponentLead)
    auto response_msg = core::KeystoneMessage::create(
        agent_id_,
        requester,
        "response",
        synthesis
    );
    response_msg.msg_id = request_msg_id;  // Preserve original msg_id for correlation

    sendMessage(response_msg);
}

void AsyncModuleLeadAgent::transitionTo(State new_state) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    current_state_ = new_state;
    execution_trace_.push_back(stateToString(new_state));
}

std::string AsyncModuleLeadAgent::stateToString(State state) const {
    switch (state) {
        case State::IDLE: return "IDLE";
        case State::PLANNING: return "PLANNING";
        case State::WAITING_FOR_TASKS: return "WAITING_FOR_TASKS";
        case State::SYNTHESIZING: return "SYNTHESIZING";
        case State::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace agents
} // namespace keystone
