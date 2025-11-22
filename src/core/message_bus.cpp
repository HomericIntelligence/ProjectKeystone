#include "core/message_bus.hpp"

#include <stdexcept>

#include "agents/agent_core.hpp"
#include "concurrency/work_stealing_scheduler.hpp"
#include "core/metrics.hpp"

namespace keystone {
namespace core {

void MessageBus::setScheduler(concurrency::WorkStealingScheduler* scheduler) {
  // FIX C5: Use atomic store for thread-safe access (no mutex needed)
  scheduler_.store(scheduler, std::memory_order_release);
}

concurrency::WorkStealingScheduler* MessageBus::getScheduler() const {
  // FIX C5: Use atomic load for thread-safe access (no mutex needed)
  return scheduler_.load(std::memory_order_acquire);
}

void MessageBus::registerAgent(const std::string& agent_id, std::shared_ptr<agents::AgentCore> agent) {
  // FIX C2: Use shared_ptr for safe lifetime management
  if (!agent) {
    throw std::invalid_argument("Cannot register null agent");
  }

  std::lock_guard<std::mutex> lock(registry_mutex_);

  // FIX P2-10: Enforce maximum agent limit to prevent DoS
  if (agents_.size() >= Config::MAX_AGENTS) {
    throw std::runtime_error("Maximum agent count exceeded: " +
                             std::to_string(Config::MAX_AGENTS));
  }

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
  // FIX C1: Deadlock prevention - lookup agent, then release lock
  // before making external calls (agent->receiveMessage or scheduler->submit)
  // FIX C2: Use shared_ptr to keep agent alive during async routing
  // FIX C5: Scheduler loaded atomically (no mutex needed)
  std::shared_ptr<agents::AgentCore> agent;

  {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = agents_.find(msg.receiver_id);
    if (it == agents_.end()) {
      return false;  // Receiver not found
    }
    agent = it->second;  // Ref count incremented - agent kept alive
  }  // ✅ Lock released before external calls

  // Load scheduler atomically (thread-safe)
  concurrency::WorkStealingScheduler* sched = scheduler_.load(std::memory_order_acquire);

  // Record message sent to metrics for tracking
  Metrics::getInstance().recordMessageSent(msg.msg_id, msg.priority);

  // Async routing if scheduler present (Phase A Week 3+)
  if (sched) {
    // FIX C2: Lambda captures shared_ptr, keeping agent alive
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
