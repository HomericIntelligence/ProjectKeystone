#include "agents/component_lead_agent.hpp"

#include <algorithm>
#include <regex>
#include <sstream>

namespace keystone {
namespace agents {

ComponentLeadAgent::ComponentLeadAgent(const std::string& agent_id) : BaseAgent(agent_id) {
  transitionTo(State::IDLE);
}

core::Response ComponentLeadAgent::processMessage(const core::KeystoneMessage& msg) {
  // Check if this is a module result or a component goal
  if (msg.command == "module_result") {
    // This is a module result - handled separately
    processModuleResult(msg);
    return core::Response::createSuccess(msg, agent_id_, "Module result processed");
  }

  // This is a component goal from ChiefArchitect
  transitionTo(State::PLANNING);

  current_component_goal_ = msg.command;
  requester_id_ = msg.sender_id;

  // Decompose component goal into module goals
  auto module_goals = decomposeModules(current_component_goal_);

  if (module_goals.empty()) {
    transitionTo(State::ERROR);
    return core::Response::createError(msg, agent_id_, "Failed to decompose component goal");
  }

  // Delegate module goals to ModuleLeadAgents
  expected_module_results_ = static_cast<int>(module_goals.size());
  received_module_results_ = 0;
  module_results_.clear();

  transitionTo(State::WAITING_FOR_MODULES);
  delegateModules(module_goals);

  return core::Response::createSuccess(msg,
                                       agent_id_,
                                       "Component goal decomposed into " +
                                           std::to_string(module_goals.size()) + " modules");
}

void ComponentLeadAgent::setAvailableModuleLeads(const std::vector<std::string>& module_lead_ids) {
  available_module_leads_ = module_lead_ids;
}

std::vector<std::string> ComponentLeadAgent::decomposeModules(const std::string& component_goal) {
  std::vector<std::string> module_goals;

  // Parse goal like "Implement Core component: Messaging(10+20+30) and Concurrency(40+50+60)"
  // Extract module sections using regex

  // Pattern to match "ModuleName(numbers)"
  std::regex module_regex(R"((\w+)\(([0-9+]+)\))");
  auto modules_begin = std::sregex_iterator(component_goal.begin(),
                                            component_goal.end(),
                                            module_regex);
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

void ComponentLeadAgent::delegateModules(const std::vector<std::string>& module_goals) {
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
    auto module_msg = core::KeystoneMessage::create(agent_id_, module_lead_id, module_goals[i]);

    // Track pending module
    pending_modules_[module_msg.msg_id] = module_lead_id;

    // Send via MessageBus
    sendMessage(module_msg);
  }
}

void ComponentLeadAgent::processModuleResult(const core::KeystoneMessage& result_msg) {
  if (!result_msg.payload) {
    // No payload, skip
    return;
  }

  // Store the module result
  module_results_.push_back(*result_msg.payload);
  received_module_results_++;

  // Check if we've received all module results
  if (received_module_results_ >= expected_module_results_) {
    transitionTo(State::AGGREGATING);
  }
}

std::string ComponentLeadAgent::synthesizeComponentResult() {
  if (current_state_ != State::AGGREGATING && current_state_ != State::WAITING_FOR_MODULES) {
    return "ERROR: Cannot synthesize in current state";
  }

  // Aggregate all module results
  std::stringstream ss;
  ss << "Component result: ";

  for (size_t i = 0; i < module_results_.size(); ++i) {
    if (i > 0) {
      ss << ", ";
    }
    ss << module_results_[i];
  }

  transitionTo(State::IDLE);

  return ss.str();
}

void ComponentLeadAgent::transitionTo(State new_state) {
  current_state_ = new_state;
  execution_trace_.push_back(stateToString(new_state));
}

std::string ComponentLeadAgent::stateToString(State state) const {
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
