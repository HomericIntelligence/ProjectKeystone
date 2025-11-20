#include "agents/module_lead_agent.hpp"

#include <algorithm>
#include <numeric>
#include <regex>
#include <sstream>

namespace keystone {
namespace agents {

ModuleLeadAgent::ModuleLeadAgent(const std::string& agent_id)
    : BaseAgent(agent_id) {
  transitionTo(State::IDLE);
}

core::Response ModuleLeadAgent::processMessage(
    const core::KeystoneMessage& msg) {
  // Check if this is a task result (from TaskAgent) or a module goal (from
  // ChiefArchitect)
  if (msg.command == "response") {
    // This is a task result - handled separately
    processTaskResult(msg);
    return core::Response::createSuccess(msg, agent_id_,
                                         "Task result processed");
  }

  // This is a module goal from ChiefArchitect
  transitionTo(State::PLANNING);

  current_module_goal_ = msg.command;
  requester_id_ = msg.sender_id;

  // Decompose module goal into tasks
  auto tasks = decomposeTasks(current_module_goal_);

  if (tasks.empty()) {
    transitionTo(State::ERROR);
    return core::Response::createError(msg, agent_id_,
                                       "Failed to decompose module goal");
  }

  // Delegate tasks to TaskAgents
  expected_results_ = static_cast<int>(tasks.size());
  received_results_ = 0;
  task_results_.clear();

  transitionTo(State::WAITING_FOR_TASKS);
  delegateTasks(tasks);

  return core::Response::createSuccess(
      msg, agent_id_,
      "Module goal decomposed into " + std::to_string(tasks.size()) + " tasks");
}

void ModuleLeadAgent::setAvailableTaskAgents(
    const std::vector<std::string>& task_agent_ids) {
  available_task_agents_ = task_agent_ids;
}

std::vector<std::string> ModuleLeadAgent::decomposeTasks(
    const std::string& module_goal) {
  std::vector<std::string> tasks;

  // Parse goal like "Calculate sum of: 10 + 20 + 30"
  // or "Calculate: 100 + 200"

  // Extract numbers using regex
  std::regex number_regex(R"(\d+)");
  auto numbers_begin = std::sregex_iterator(module_goal.begin(),
                                            module_goal.end(), number_regex);
  auto numbers_end = std::sregex_iterator();

  // Create a task for each number (echo the number)
  for (std::sregex_iterator i = numbers_begin; i != numbers_end; ++i) {
    std::string number = i->str();
    std::string task = "echo " + number;
    tasks.push_back(task);
  }

  return tasks;
}

void ModuleLeadAgent::delegateTasks(const std::vector<std::string>& tasks) {
  // Assign tasks to available TaskAgents in round-robin fashion
  for (size_t i = 0; i < tasks.size(); ++i) {
    if (available_task_agents_.empty()) {
      throw std::runtime_error("No TaskAgents available for delegation");
    }

    // Round-robin assignment
    size_t agent_index = i % available_task_agents_.size();
    const std::string& task_agent_id = available_task_agents_[agent_index];

    // Create and send task message
    auto task_msg =
        core::KeystoneMessage::create(agent_id_, task_agent_id, tasks[i]);

    // Track pending task
    pending_tasks_[task_msg.msg_id] = task_agent_id;

    // Send via MessageBus
    sendMessage(task_msg);
  }
}

void ModuleLeadAgent::processTaskResult(
    const core::KeystoneMessage& result_msg) {
  if (!result_msg.payload) {
    // No payload, skip
    return;
  }

  // Store the result
  task_results_.push_back(*result_msg.payload);
  received_results_++;

  // Check if we've received all results
  if (received_results_ >= expected_results_) {
    transitionTo(State::SYNTHESIZING);
  }
}

std::string ModuleLeadAgent::synthesizeResults() {
  if (current_state_ != State::SYNTHESIZING &&
      current_state_ != State::WAITING_FOR_TASKS) {
    return "ERROR: Cannot synthesize in current state";
  }

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

  transitionTo(State::IDLE);

  // Return synthesis
  std::stringstream ss;
  ss << "Module result: Sum = " << total;
  return ss.str();
}

void ModuleLeadAgent::transitionTo(State new_state) {
  current_state_ = new_state;
  execution_trace_.push_back(stateToString(new_state));
}

std::string ModuleLeadAgent::stateToString(State state) const {
  switch (state) {
    case State::IDLE:
      return "IDLE";
    case State::PLANNING:
      return "PLANNING";
    case State::WAITING_FOR_TASKS:
      return "WAITING_FOR_TASKS";
    case State::SYNTHESIZING:
      return "SYNTHESIZING";
    case State::ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

}  // namespace agents
}  // namespace keystone
