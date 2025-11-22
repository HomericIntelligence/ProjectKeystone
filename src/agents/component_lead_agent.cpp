#include "agents/component_lead_agent.hpp"

#include <algorithm>
#include <chrono>
#include <regex>
#include <sstream>
#include <thread>

#ifdef ENABLE_GRPC
#include "hmas_coordinator.pb.h"
#endif

namespace keystone {
namespace agents {

ComponentLeadAgent::ComponentLeadAgent(const std::string& agent_id) : AsyncAgent(agent_id) {
  transitionTo(State::IDLE);
}

concurrency::Task<core::Response> ComponentLeadAgent::processMessage(
    const core::KeystoneMessage& msg) {
  // FIX C3: Changed to async (returns Task<Response>)
  // Check if this is a module result or a component goal
  if (msg.command == "module_result") {
    // This is a module result - handled separately
    processModuleResult(msg);
    co_return core::Response::createSuccess(msg, agent_id_, "Module result processed");
  }

  // This is a component goal from ChiefArchitect
  transitionTo(State::PLANNING);

  current_component_goal_ = msg.command;
  requester_id_ = msg.sender_id;

  // Decompose component goal into module goals
  auto module_goals = decomposeModules(current_component_goal_);

  if (module_goals.empty()) {
    transitionTo(State::ERROR);
    co_return core::Response::createError(msg, agent_id_, "Failed to decompose component goal");
  }

  // Delegate module goals to ModuleLeadAgents
  expected_module_results_ = static_cast<int>(module_goals.size());
  received_module_results_ = 0;
  module_results_.clear();

  transitionTo(State::WAITING_FOR_MODULES);
  delegateModules(module_goals);

  co_return core::Response::createSuccess(msg,
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

#ifdef ENABLE_GRPC
void ComponentLeadAgent::initializeGrpc(const std::string& coordinator_address,
                                        const std::string& registry_address,
                                        const std::string& agent_type,
                                        int level) {
  agent_type_ = agent_type;
  agent_level_ = level;

  // Create gRPC clients
  network::GrpcClientConfig coordinator_config;
  coordinator_config.server_address = coordinator_address;
  coordinator_client_ =
      std::make_unique<network::HMASCoordinatorClient>(coordinator_config);

  network::GrpcClientConfig registry_config;
  registry_config.server_address = registry_address;
  registry_client_ =
      std::make_unique<network::ServiceRegistryClient>(registry_config);

  // Register with ServiceRegistry
  hmas::AgentRegistration registration;
  registration.set_agent_id(agent_id_);
  registration.set_agent_type(agent_type_);
  registration.set_level(level);
  registration.add_capabilities("module_coordination");
  registration.add_capabilities("component_synthesis");
  registration.set_max_concurrent_tasks(10);

  try {
    auto response = registry_client_->registerAgent(registration);
    if (response.success()) {
      startHeartbeat();
    } else {
      throw std::runtime_error("Failed to register with ServiceRegistry: " +
                               response.message());
    }
  } catch (const std::exception& e) {
    throw std::runtime_error("gRPC registration failed: " +
                             std::string(e.what()));
  }
}

void ComponentLeadAgent::processYamlComponent(const std::string& yaml_spec) {
  auto spec_opt = network::YamlParser::parseTaskSpec(yaml_spec);
  if (!spec_opt) {
    throw std::runtime_error("Failed to parse YAML component specification");
  }

  auto spec = *spec_opt;
  current_task_id_ = spec.metadata.task_id;

  transitionTo(State::PLANNING);

  // Decompose component into modules
  auto module_goals =
      decomposeModules(spec.hierarchy.level1_directive.value_or(""));

  if (module_goals.empty()) {
    spec.status.phase = "FAILED";
    spec.status.error = "Failed to decompose component goal";
    transitionTo(State::ERROR);

    std::string result_yaml = network::YamlParser::generateTaskSpec(spec);
    if (coordinator_client_ && spec.metadata.parent_task_id) {
      hmas::TaskResult task_result;
      task_result.set_task_id(spec.metadata.task_id);
      task_result.set_result_yaml(result_yaml);
      task_result.set_success(false);
      task_result.set_error_message(*spec.status.error);
      coordinator_client_->submitResult(task_result);
    }
    return;
  }

  // Query for available ModuleLeadAgents
  available_module_leads_ = queryAvailableModuleLeads();

  if (available_module_leads_.empty()) {
    spec.status.phase = "FAILED";
    spec.status.error = "No ModuleLeadAgents available";
    transitionTo(State::ERROR);

    std::string result_yaml = network::YamlParser::generateTaskSpec(spec);
    if (coordinator_client_ && spec.metadata.parent_task_id) {
      hmas::TaskResult task_result;
      task_result.set_task_id(spec.metadata.task_id);
      task_result.set_result_yaml(result_yaml);
      task_result.set_success(false);
      task_result.set_error_message(*spec.status.error);
      coordinator_client_->submitResult(task_result);
    }
    return;
  }

  // Create result aggregator
  auto timeout = network::YamlParser::parseDuration(spec.aggregation.timeout);
  result_aggregator_ = std::make_unique<network::ResultAggregator>(
      network::stringToStrategy(spec.aggregation.strategy),
      std::chrono::milliseconds(timeout.value_or(25 * 60 * 1000)),
      module_goals.size());

  // Generate child module YAMLs and submit to ModuleLeadAgents
  transitionTo(State::WAITING_FOR_MODULES);
  expected_module_results_ = static_cast<int>(module_goals.size());
  received_module_results_ = 0;
  module_results_.clear();

  for (size_t i = 0; i < module_goals.size(); ++i) {
    // Create child module spec
    network::HierarchicalTaskSpec child_spec;
    child_spec.api_version = "v1";
    child_spec.kind = "HierarchicalTask";
    child_spec.metadata.name = "module-" + std::to_string(i);
    child_spec.metadata.task_id =
        spec.metadata.task_id + "-module-" + std::to_string(i);
    child_spec.metadata.parent_task_id = spec.metadata.task_id;
    child_spec.metadata.session_id = spec.metadata.session_id;

    // Set routing
    size_t agent_index = i % available_module_leads_.size();
    child_spec.routing.target_agent_id = available_module_leads_[agent_index];
    child_spec.routing.target_level = 2;
    child_spec.routing.target_agent_type = "ModuleLeadAgent";

    // Set hierarchy
    child_spec.hierarchy.level0_goal = spec.hierarchy.level0_goal;
    child_spec.hierarchy.level1_directive = spec.hierarchy.level1_directive;
    child_spec.hierarchy.level2_module = module_goals[i];

    // Set action
    child_spec.action.type = "DECOMPOSE";

    // Copy aggregation config
    child_spec.aggregation = spec.aggregation;

    // Generate YAML
    std::string child_yaml = network::YamlParser::generateTaskSpec(child_spec);

    // Submit module via gRPC
    try {
      auto deadline_ms = network::YamlParser::parseDuration(
                             spec.metadata.deadline.value_or("25m"))
                             .value_or(25 * 60 * 1000);
      auto response = coordinator_client_->submitTask(
          child_yaml, spec.metadata.session_id.value_or(""), deadline_ms,
          hmas::TASK_PRIORITY_NORMAL, spec.metadata.task_id);

      pending_modules_[child_spec.metadata.task_id] =
          available_module_leads_[agent_index];
    } catch (const std::exception& e) {
      std::cerr << "Failed to submit module: " << e.what() << std::endl;
    }
  }
}

void ComponentLeadAgent::startHeartbeat() {
  if (heartbeat_running_) {
    return;
  }

  heartbeat_running_ = true;
  heartbeat_thread_ = std::thread(&ComponentLeadAgent::heartbeatLoop, this);
}

void ComponentLeadAgent::stopHeartbeat() {
  if (!heartbeat_running_) {
    return;
  }

  heartbeat_running_ = false;
  if (heartbeat_thread_.joinable()) {
    heartbeat_thread_.join();
  }
}

void ComponentLeadAgent::heartbeatLoop() {
  while (heartbeat_running_) {
    try {
      if (registry_client_) {
        registry_client_->heartbeat(agent_id_, 0.0f, 0.0f,
                                    expected_module_results_);
      }
    } catch (const std::exception& e) {
      std::cerr << "Heartbeat failed: " << e.what() << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void ComponentLeadAgent::shutdown() {
  stopHeartbeat();

  if (registry_client_) {
    try {
      registry_client_->unregisterAgent(agent_id_, "Shutdown requested");
    } catch (const std::exception& e) {
      std::cerr << "Failed to unregister agent: " << e.what() << std::endl;
    }
  }

  coordinator_client_.reset();
  registry_client_.reset();
  result_aggregator_.reset();
}

std::vector<std::string> ComponentLeadAgent::queryAvailableModuleLeads() {
  std::vector<std::string> agent_ids;

  if (!registry_client_) {
    return agent_ids;
  }

  try {
    hmas::AgentQuery query;
    query.set_agent_type("ModuleLeadAgent");
    query.set_level(2);
    query.set_only_alive(true);

    auto agent_list = registry_client_->queryAgents(query);

    for (const auto& agent : agent_list.agents()) {
      agent_ids.push_back(agent.agent_id());
    }
  } catch (const std::exception& e) {
    std::cerr << "Failed to query ModuleLeadAgents: " << e.what() << std::endl;
  }

  return agent_ids;
}
#endif

}  // namespace agents
}  // namespace keystone
