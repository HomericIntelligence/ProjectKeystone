#include "agents/module_lead_agent.hpp"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <regex>
#include <sstream>
#include <thread>

#ifdef ENABLE_GRPC
#include "hmas_coordinator.pb.h"
#endif

namespace keystone {
namespace agents {

ModuleLeadAgent::ModuleLeadAgent(const std::string& agent_id) : BaseAgent(agent_id) {
  transitionTo(State::IDLE);
}

concurrency::Task<core::Response> ModuleLeadAgent::processMessage(const core::KeystoneMessage& msg) {
  // FIX C3: Changed to async (returns Task<Response>)
  // Check if this is a task result (from TaskAgent) or a module goal (from ChiefArchitect)
  if (msg.command == "response") {
    // This is a task result - handled separately
    processTaskResult(msg);
    co_return core::Response::createSuccess(msg, agent_id_, "Task result processed");
  }

  // This is a module goal from ChiefArchitect
  transitionTo(State::PLANNING);

  current_module_goal_ = msg.command;
  requester_id_ = msg.sender_id;

  // Decompose module goal into tasks
  auto tasks = decomposeTasks(current_module_goal_);

  if (tasks.empty()) {
    transitionTo(State::ERROR);
    co_return core::Response::createError(msg, agent_id_, "Failed to decompose module goal");
  }

  // Delegate tasks to TaskAgents
  expected_results_ = static_cast<int>(tasks.size());
  received_results_ = 0;
  task_results_.clear();

  transitionTo(State::WAITING_FOR_TASKS);
  delegateTasks(tasks);

  co_return core::Response::createSuccess(
      msg, agent_id_, "Module goal decomposed into " + std::to_string(tasks.size()) + " tasks");
}

void ModuleLeadAgent::setAvailableTaskAgents(const std::vector<std::string>& task_agent_ids) {
  available_task_agents_ = task_agent_ids;
}

std::vector<std::string> ModuleLeadAgent::decomposeTasks(const std::string& module_goal) {
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
    auto task_msg = core::KeystoneMessage::create(agent_id_, task_agent_id, tasks[i]);

    // Track pending task
    pending_tasks_[task_msg.msg_id] = task_agent_id;

    // Send via MessageBus
    sendMessage(task_msg);
  }
}

void ModuleLeadAgent::processTaskResult(const core::KeystoneMessage& result_msg) {
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
  if (current_state_ != State::SYNTHESIZING && current_state_ != State::WAITING_FOR_TASKS) {
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

#ifdef ENABLE_GRPC
void ModuleLeadAgent::initializeGrpc(const std::string& coordinator_address,
                                     const std::string& registry_address,
                                     const std::string& agent_type, int level) {
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
  registration.add_capabilities("task_coordination");
  registration.add_capabilities("result_synthesis");
  registration.set_max_concurrent_tasks(5);

  try {
    auto response = registry_client_->registerAgent(registration);
    if (response.success()) {
      // Start heartbeat thread
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

void ModuleLeadAgent::processYamlModule(const std::string& yaml_spec) {
  // Parse YAML module specification
  auto spec_opt = network::YamlParser::parseTaskSpec(yaml_spec);
  if (!spec_opt) {
    throw std::runtime_error("Failed to parse YAML module specification");
  }

  auto spec = *spec_opt;
  current_task_id_ = spec.metadata.task_id;

  transitionTo(State::PLANNING);

  // Decompose module into tasks
  auto tasks = decomposeTasks(spec.hierarchy.level2_module.value_or(""));

  if (tasks.empty()) {
    spec.status.phase = "FAILED";
    spec.status.error = "Failed to decompose module goal";
    transitionTo(State::ERROR);

    // Generate error result YAML
    std::string result_yaml = network::YamlParser::generateTaskSpec(spec);

    // Submit error result
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

  // Query for available TaskAgents
  available_task_agents_ = queryAvailableTaskAgents();

  if (available_task_agents_.empty()) {
    spec.status.phase = "FAILED";
    spec.status.error = "No TaskAgents available";
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
      tasks.size());

  // Generate child task YAMLs and submit to TaskAgents
  transitionTo(State::WAITING_FOR_TASKS);
  expected_results_ = static_cast<int>(tasks.size());
  received_results_ = 0;
  task_results_.clear();

  for (size_t i = 0; i < tasks.size(); ++i) {
    // Create child task spec
    network::HierarchicalTaskSpec child_spec;
    child_spec.api_version = "v1";
    child_spec.kind = "HierarchicalTask";
    child_spec.metadata.name = "task-" + std::to_string(i);
    child_spec.metadata.task_id =
        spec.metadata.task_id + "-subtask-" + std::to_string(i);
    child_spec.metadata.parent_task_id = spec.metadata.task_id;
    child_spec.metadata.session_id = spec.metadata.session_id;

    // Set routing
    size_t agent_index = i % available_task_agents_.size();
    child_spec.routing.target_agent_id = available_task_agents_[agent_index];
    child_spec.routing.target_level = 3;
    child_spec.routing.target_agent_type = "TaskAgent";

    // Set payload
    child_spec.payload.command = tasks[i];

    // Set action
    child_spec.action.type = "EXECUTE";

    // Set hierarchy
    child_spec.hierarchy.level0_goal = spec.hierarchy.level0_goal;
    child_spec.hierarchy.level2_module = spec.hierarchy.level2_module;
    child_spec.hierarchy.level3_task = tasks[i];

    // Generate YAML
    std::string child_yaml = network::YamlParser::generateTaskSpec(child_spec);

    // Submit task via gRPC
    try {
      auto deadline_ms = network::YamlParser::parseDuration(
                             spec.metadata.deadline.value_or("25m"))
                             .value_or(25 * 60 * 1000);
      auto response = coordinator_client_->submitTask(
          child_yaml, spec.metadata.session_id.value_or(""), deadline_ms,
          hmas::TASK_PRIORITY_NORMAL, spec.metadata.task_id);

      pending_tasks_[child_spec.metadata.task_id] =
          available_task_agents_[agent_index];
    } catch (const std::exception& e) {
      std::cerr << "Failed to submit task: " << e.what() << std::endl;
    }
  }

  // Wait for results (in a real implementation, this would be async)
  // For now, we'll rely on processTaskResult being called externally
}

void ModuleLeadAgent::startHeartbeat() {
  if (heartbeat_running_) {
    return;
  }

  heartbeat_running_ = true;
  heartbeat_thread_ = std::thread(&ModuleLeadAgent::heartbeatLoop, this);
}

void ModuleLeadAgent::stopHeartbeat() {
  if (!heartbeat_running_) {
    return;
  }

  heartbeat_running_ = false;
  if (heartbeat_thread_.joinable()) {
    heartbeat_thread_.join();
  }
}

void ModuleLeadAgent::heartbeatLoop() {
  while (heartbeat_running_) {
    try {
      if (registry_client_) {
        registry_client_->heartbeat(agent_id_, 0.0f, 0.0f, expected_results_);
      }
    } catch (const std::exception& e) {
      std::cerr << "Heartbeat failed: " << e.what() << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void ModuleLeadAgent::shutdown() {
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

std::vector<std::string> ModuleLeadAgent::queryAvailableTaskAgents() {
  std::vector<std::string> agent_ids;

  if (!registry_client_) {
    return agent_ids;
  }

  try {
    hmas::AgentQuery query;
    query.set_agent_type("TaskAgent");
    query.set_level(3);
    query.set_only_alive(true);

    auto agent_list = registry_client_->queryAgents(query);

    for (const auto& agent : agent_list.agents()) {
      agent_ids.push_back(agent.agent_id());
    }
  } catch (const std::exception& e) {
    std::cerr << "Failed to query TaskAgents: " << e.what() << std::endl;
  }

  return agent_ids;
}
#endif

}  // namespace agents
}  // namespace keystone
