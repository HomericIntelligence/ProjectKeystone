#include "core/message_bus.hpp"

#include "agents/agent_base.hpp"
#include "concurrency/work_stealing_scheduler.hpp"
#include "core/metrics.hpp"

#include <stdexcept>

namespace keystone {
namespace core {

void MessageBus::setScheduler(concurrency::WorkStealingScheduler* scheduler) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  scheduler_ = scheduler;
}

concurrency::WorkStealingScheduler* MessageBus::getScheduler() const {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  return scheduler_;
}

void MessageBus::registerAgent(const std::string& agent_id, agents::AgentBase* agent) {
  if (!agent) {
    throw std::invalid_argument("Cannot register null agent");
  }

  std::lock_guard<std::mutex> lock(registry_mutex_);

  if (agents_.find(agent_id) != agents_.end()) {
    throw std::runtime_error("Agent ID already registered: " + agent_id);
  }

  agents_[agent_id] = agent;
}

void MessageBus::unregisterAgent(const std::string& agent_id) {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  agents_.erase(agent_id);
}

bool MessageBus::routeMessage(const KeystoneMessage& msg) {
  // FIX C1: Deadlock prevention - lookup agent/scheduler, then release lock
  // before making external calls (agent->receiveMessage or scheduler->submit)
  agents::AgentBase* agent = nullptr;
  concurrency::WorkStealingScheduler* sched = nullptr;

  {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = agents_.find(msg.receiver_id);
    if (it == agents_.end()) {
      return false;  // Receiver not found
    }
    agent = it->second;
    sched = scheduler_;
  }  // ✅ Lock released before external calls

  // Record message sent to metrics for tracking
  Metrics::getInstance().recordMessageSent(msg.msg_id, msg.priority);

  // Async routing if scheduler present (Phase A Week 3+)
  if (sched) {
    // Submit message delivery as work item to scheduler
    sched->submit([agent, msg]() { agent->receiveMessage(msg); });
    return true;
  }

  // Synchronous delivery for backward compatibility (Phase 1-3)
  agent->receiveMessage(msg);
  return true;
}

bool MessageBus::hasAgent(const std::string& agent_id) const {
  std::lock_guard<std::mutex> lock(registry_mutex_);
  return agents_.find(agent_id) != agents_.end();
}

std::vector<std::string> MessageBus::listAgents() const {
  std::lock_guard<std::mutex> lock(registry_mutex_);

  std::vector<std::string> agent_ids;
  agent_ids.reserve(agents_.size());

  for (const auto& [id, _] : agents_) {
    agent_ids.push_back(id);
  }

  return agent_ids;
}

}  // namespace core
}  // namespace keystone
