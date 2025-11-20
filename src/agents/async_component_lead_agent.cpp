#include "agents/async_component_lead_agent.hpp"

#include <regex>
#include <sstream>

namespace keystone {
namespace agents {

AsyncComponentLeadAgent::AsyncComponentLeadAgent(const std::string& agent_id)
    : AsyncBaseAgent(agent_id) {
  transitionTo(State::IDLE);
}

concurrency::Task<core::Response> AsyncComponentLeadAgent::processMessage(
    const core::KeystoneMessage& msg) {
  // Check if this is a module result or a component goal
  // Async version uses "response" for consistency (sync version used
  // "module_result")
  if (msg.command == "response" || msg.command == "module_result") {
    // This is a module result - process asynchronously
    co_await processModuleResult(msg);
    auto response = core::Response::createSuccess(msg, agent_id_,
                                                  "Module result processed");
    co_return response;
  }

  // This is a component goal from ChiefArchitect
  transitionTo(State::PLANNING);

  {
    std::lock_guard<std::mutex> lock(module_mutex_);
    current_component_goal_ = msg.command;
    requester_id_ = msg.sender_id;
    current_request_msg_id_ = msg.msg_id;
  }

  // Decompose component goal into module goals
  auto module_goals = decomposeModules(msg.command);

  if (module_goals.empty()) {
    transitionTo(State::ERROR);
    auto response = core::Response::createError(
        msg, agent_id_, "Failed to decompose component goal");
    co_return response;
  }

  // Delegate module goals to ModuleLeadAgents
  {
    std::lock_guard<std::mutex> lock(module_mutex_);
    expected_module_results_ = static_cast<int>(module_goals.size());
    received_module_results_ = 0;
    module_results_.clear();
  }

  transitionTo(State::WAITING_FOR_MODULES);
  delegateModules(module_goals);

  auto response = core::Response::createSuccess(
      msg, agent_id_,
      "Component goal decomposed into " + std::to_string(module_goals.size()) +
          " modules");
  co_return response;
}

void AsyncComponentLeadAgent::setAvailableModuleLeads(
    const std::vector<std::string>& module_lead_ids) {
  std::lock_guard<std::mutex> lock(module_mutex_);
  available_module_leads_ = module_lead_ids;
}

concurrency::Task<void> AsyncComponentLeadAgent::processModuleResult(
    const core::KeystoneMessage& result_msg) {
  if (!result_msg.payload) {
    co_return;
  }

  bool all_received = false;
  {
    std::lock_guard<std::mutex> lock(module_mutex_);

    // Store the module result
    module_results_.push_back(*result_msg.payload);
    received_module_results_++;

    // Check if we've received all module results
    if (received_module_results_ >= expected_module_results_) {
      transitionTo(State::AGGREGATING);
      all_received = true;
    }
  }

  // If all results received, aggregate and respond
  if (all_received) {
    aggregateAndRespond();
  }

  co_return;
}

std::vector<std::string> AsyncComponentLeadAgent::decomposeModules(
    const std::string& component_goal) {
  std::vector<std::string> module_goals;

  // Parse goal like "Implement Core component: Messaging(10+20+30) and
  // Concurrency(40+50+60)" Extract module sections using regex

  // Pattern to match "ModuleName(numbers)"
  std::regex module_regex(R"((\w+)\(([0-9+]+)\))");
  auto modules_begin = std::sregex_iterator(component_goal.begin(),
                                            component_goal.end(), module_regex);
  auto modules_end = std::sregex_iterator();

  for (std::sregex_iterator i = modules_begin; i != modules_end; ++i) {
    std::smatch match = *i;
    std::string module_name = match[1].str();  // e.g., "Messaging"
    std::string numbers = match[2].str();      // e.g., "10+20+30"

    // Create module goal: "Calculate MODULE: numbers"
    std::string module_goal = "Calculate " + module_name + ": " + numbers;
    module_goals.push_back(module_goal);
  }

  return module_goals;
}

void AsyncComponentLeadAgent::delegateModules(
    const std::vector<std::string>& module_goals) {
  std::lock_guard<std::mutex> lock(module_mutex_);

  // Assign module goals to available ModuleLeadAgents
  for (size_t i = 0; i < module_goals.size(); ++i) {
    if (available_module_leads_.empty()) {
      throw std::runtime_error("No ModuleLeadAgents available for delegation");
    }

    if (i >= available_module_leads_.size()) {
      throw std::runtime_error("Not enough ModuleLeadAgents for all modules");
    }

    // Assign one module per ModuleLead
    const std::string& module_lead_id = available_module_leads_[i];

    // Create and send module goal message
    auto module_msg = core::KeystoneMessage::create(agent_id_, module_lead_id,
                                                    module_goals[i]);

    // Track pending module
    pending_modules_[module_msg.msg_id] = module_lead_id;

    // Send via MessageBus (async routing)
    sendMessage(module_msg);
  }
}

void AsyncComponentLeadAgent::aggregateAndRespond() {
  std::string aggregation;
  std::string requester;
  std::string request_msg_id;

  {
    std::lock_guard<std::mutex> lock(module_mutex_);

    // Aggregate all module results
    std::stringstream ss;
    ss << "Component result: ";

    for (size_t i = 0; i < module_results_.size(); ++i) {
      if (i > 0) {
        ss << ", ";
      }
      ss << module_results_[i];
    }

    aggregation = ss.str();
    requester = requester_id_;
    request_msg_id = current_request_msg_id_;
  }

  transitionTo(State::IDLE);

  // Send aggregated result back to requester (ChiefArchitect)
  auto response_msg = core::KeystoneMessage::create(agent_id_, requester,
                                                    "response", aggregation);
  response_msg.msg_id =
      request_msg_id;  // Preserve original msg_id for correlation

  sendMessage(response_msg);
}

void AsyncComponentLeadAgent::transitionTo(State new_state) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  current_state_ = new_state;
  execution_trace_.push_back(stateToString(new_state));
}

std::string AsyncComponentLeadAgent::stateToString(State state) const {
  switch (state) {
    case State::IDLE:
      return "IDLE";
    case State::PLANNING:
      return "PLANNING";
    case State::WAITING_FOR_MODULES:
      return "WAITING_FOR_MODULES";
    case State::AGGREGATING:
      return "AGGREGATING";
    case State::ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

}  // namespace agents
}  // namespace keystone
