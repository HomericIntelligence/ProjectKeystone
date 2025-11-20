#include "agents/async_chief_architect_agent.hpp"

#include <algorithm>
#include <stdexcept>

namespace keystone {
namespace agents {

AsyncChiefArchitectAgent::AsyncChiefArchitectAgent(const std::string& agent_id)
    : AsyncBaseAgent(agent_id) {}

concurrency::Task<core::Response> AsyncChiefArchitectAgent::processMessage(
    const core::KeystoneMessage& msg) {
  // Skip response messages to avoid feedback loops (similar to AsyncTaskAgent)
  if (msg.command == "response") {
    auto response =
        core::Response::createSuccess(msg, agent_id_, "response received");
    co_return response;
  }

  // ChiefArchitect receives responses from subordinates (TaskAgent, ModuleLead,
  // etc.) Check payload exists
  if (!msg.payload) {
    auto response = core::Response::createError(msg, agent_id_,
                                                "Missing payload in message");
    co_return response;
  }

  // Create success response with payload
  auto response = core::Response::createSuccess(msg, agent_id_, *msg.payload);
  co_return response;
}

void AsyncChiefArchitectAgent::sendCommandAsync(
    const std::string& command, const std::string& subordinate_id) {
  // Create message
  auto msg = core::KeystoneMessage::create(agent_id_, subordinate_id, command);

  // Track pending command for response correlation
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_commands_[msg.msg_id] = command;
  }

  // Send via MessageBus (async routing if scheduler set)
  sendMessage(msg);
}

// ============================================================================
// PHASE 4: Multi-Component Coordination Implementation
// ============================================================================

void AsyncChiefArchitectAgent::registerComponent(
    const std::string& component_name,
    AsyncComponentLeadAgent* component_lead) {
  std::lock_guard<std::mutex> lock(component_mutex_);
  component_registry_[component_name] = component_lead;
}

void AsyncChiefArchitectAgent::addComponentDependency(
    const std::string& component_name,
    const std::vector<std::string>& dependencies) {
  std::lock_guard<std::mutex> lock(component_mutex_);
  component_dependencies_[component_name] = dependencies;
}

std::vector<std::string> AsyncChiefArchitectAgent::getComponentNames() const {
  std::lock_guard<std::mutex> lock(component_mutex_);
  std::vector<std::string> names;
  names.reserve(component_registry_.size());
  for (const auto& [name, _] : component_registry_) {
    names.push_back(name);
  }
  return names;
}

std::vector<std::string> AsyncChiefArchitectAgent::getComponentExecutionOrder()
    const {
  std::lock_guard<std::mutex> lock(component_mutex_);

  std::unordered_set<std::string> visited;
  std::unordered_set<std::string> rec_stack;
  std::vector<std::string> result;

  // Run topological sort for each component
  for (const auto& [component_name, _] : component_registry_) {
    if (visited.find(component_name) == visited.end()) {
      if (!topologicalSortUtil(visited, rec_stack, component_name, result)) {
        throw std::runtime_error(
            "Circular dependency detected in component dependencies");
      }
    }
  }

  // DFS post-order already gives correct topological order (dependencies first)
  // No need to reverse
  return result;
}

bool AsyncChiefArchitectAgent::topologicalSortUtil(
    std::unordered_set<std::string>& visited,
    std::unordered_set<std::string>& rec_stack, const std::string& component,
    std::vector<std::string>& result) const {
  // Mark current node as visited and add to recursion stack
  visited.insert(component);
  rec_stack.insert(component);

  // Check dependencies of this component
  auto dep_it = component_dependencies_.find(component);
  if (dep_it != component_dependencies_.end()) {
    for (const auto& dependency : dep_it->second) {
      // If dependency not visited, recurse
      if (visited.find(dependency) == visited.end()) {
        if (!topologicalSortUtil(visited, rec_stack, dependency, result)) {
          return false;  // Cycle detected
        }
      }
      // If dependency is in recursion stack, we have a cycle
      else if (rec_stack.find(dependency) != rec_stack.end()) {
        return false;  // Circular dependency
      }
    }
  }

  // Remove from recursion stack and add to result
  rec_stack.erase(component);
  result.push_back(component);
  return true;
}

std::vector<std::vector<std::string>>
AsyncChiefArchitectAgent::getComponentDependencyLevels() const {
  std::lock_guard<std::mutex> lock(component_mutex_);

  // Build in-degree map for Kahn's algorithm
  std::unordered_map<std::string, int> in_degree;
  for (const auto& [component_name, _] : component_registry_) {
    in_degree[component_name] = 0;
  }

  // Calculate in-degrees
  for (const auto& [component, dependencies] : component_dependencies_) {
    in_degree[component] += dependencies.size();
  }

  std::vector<std::vector<std::string>> levels;
  std::unordered_set<std::string> processed;

  // Process components level by level
  while (processed.size() < component_registry_.size()) {
    std::vector<std::string> current_level;

    // Find all components with in-degree 0 (no unprocessed dependencies)
    for (const auto& [component_name, _] : component_registry_) {
      if (processed.find(component_name) != processed.end()) {
        continue;  // Already processed
      }

      // Check if all dependencies are processed
      bool all_deps_processed = true;
      auto dep_it = component_dependencies_.find(component_name);
      if (dep_it != component_dependencies_.end()) {
        for (const auto& dep : dep_it->second) {
          if (processed.find(dep) == processed.end()) {
            all_deps_processed = false;
            break;
          }
        }
      }

      if (all_deps_processed) {
        current_level.push_back(component_name);
      }
    }

    if (current_level.empty() &&
        processed.size() < component_registry_.size()) {
      // Circular dependency detected
      throw std::runtime_error(
          "Circular dependency detected in component dependencies");
    }

    // Add current level
    levels.push_back(current_level);

    // Mark as processed
    for (const auto& component : current_level) {
      processed.insert(component);
    }
  }

  return levels;
}

concurrency::Task<std::vector<ComponentResult>>
AsyncChiefArchitectAgent::executeAllComponents(const std::string& goal) {
  // Get dependency levels for parallel execution
  std::vector<std::vector<std::string>> levels;
  try {
    levels = getComponentDependencyLevels();
  } catch (const std::runtime_error& e) {
    // Circular dependency detected - return error
    ComponentResult error_result;
    error_result.component_name = "SYSTEM";
    error_result.success = false;
    error_result.error_message = e.what();
    co_return std::vector<ComponentResult>{error_result};
  }

  std::vector<ComponentResult> results;

  // Execute each level in parallel
  for (const auto& level : levels) {
    std::vector<concurrency::Task<ComponentResult>> level_tasks;

    // Submit all components in this level in parallel
    for (const auto& component_name : level) {
      // Find component in registry
      AsyncComponentLeadAgent* component_lead = nullptr;
      {
        std::lock_guard<std::mutex> lock(component_mutex_);
        auto it = component_registry_.find(component_name);
        if (it != component_registry_.end()) {
          component_lead = it->second;
        }
      }

      if (!component_lead) {
        ComponentResult error_result;
        error_result.component_name = component_name;
        error_result.success = false;
        error_result.error_message = "Component not found in registry";
        results.push_back(error_result);
        continue;
      }

      // Create task for this component
      auto task = [](AsyncChiefArchitectAgent* chief,
                     const std::string& comp_name,
                     AsyncComponentLeadAgent* lead, const std::string& goal_str)
          -> concurrency::Task<ComponentResult> {
        ComponentResult comp_result;
        comp_result.component_name = comp_name;

        // Send command to component
        chief->sendCommandAsync(goal_str, lead->getAgentId());

        // Mark as success (simplified - in real impl, would await response)
        comp_result.success = true;
        comp_result.modules_completed = 0;
        comp_result.tasks_completed = 0;

        co_return comp_result;
      }(this, component_name, component_lead, goal);

      level_tasks.push_back(std::move(task));
    }

    // Await all tasks in this level to complete
    for (auto& task : level_tasks) {
      auto result = co_await task;
      results.push_back(result);
    }
  }

  co_return results;
}

}  // namespace agents
}  // namespace keystone
