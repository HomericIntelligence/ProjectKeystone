#include "simulation/simulated_numa_node.hpp"

#include "concurrency/logger.hpp"

#include <stdexcept>

namespace keystone {
namespace simulation {

using namespace concurrency;

SimulatedNUMANode::SimulatedNUMANode(size_t node_id, size_t num_workers)
    : node_id_(node_id), scheduler_(std::make_unique<WorkStealingScheduler>(num_workers)) {
  Logger::debug("SimulatedNUMANode {}: Created with {} workers", node_id_, num_workers);
}

SimulatedNUMANode::~SimulatedNUMANode() {
  if (scheduler_) {
    scheduler_->shutdown();
  }
  Logger::debug("SimulatedNUMANode {}: Destroyed", node_id_);
}

void SimulatedNUMANode::start() {
  scheduler_->start();
  Logger::info("SimulatedNUMANode {}: Started", node_id_);
}

void SimulatedNUMANode::shutdown() {
  scheduler_->shutdown();
  Logger::info("SimulatedNUMANode {}: Shutdown (local_steals={}, remote_steals={})",
               node_id_,
               local_steals_.load(),
               remote_steals_.load());
}

void SimulatedNUMANode::submit(std::function<void()> work) {
  scheduler_->submit(std::move(work));
}

void SimulatedNUMANode::submitToWorker(size_t worker_index, std::function<void()> work) {
  scheduler_->submitTo(worker_index, std::move(work));
}

void SimulatedNUMANode::registerAgent(const std::string& agent_id) {
  local_agents_.insert(agent_id);
  Logger::debug("SimulatedNUMANode {}: Registered agent '{}'", node_id_, agent_id);
}

void SimulatedNUMANode::unregisterAgent(const std::string& agent_id) {
  local_agents_.erase(agent_id);
  Logger::debug("SimulatedNUMANode {}: Unregistered agent '{}'", node_id_, agent_id);
}

bool SimulatedNUMANode::hasAgent(const std::string& agent_id) const {
  return local_agents_.find(agent_id) != local_agents_.end();
}

std::optional<std::function<void()>> SimulatedNUMANode::stealWork() {
  // Try to steal from a random worker's queue
  // In real work-stealing, this would be the scheduler's responsibility
  // For simulation, we'll delegate to the scheduler's internal mechanism

  // Note: This is a simplified steal - in practice, we'd need to expose
  // the scheduler's steal mechanism or modify it to support cross-node stealing
  // For now, we'll just increment the counter and return nullopt
  // TODO: Implement actual work stealing from scheduler queues

  remote_steals_++;
  return std::nullopt;  // Simplified for initial implementation
}

void SimulatedNUMANode::recordLocalSteal() {
  local_steals_++;
}

size_t SimulatedNUMANode::getNumWorkers() const {
  return scheduler_->getNumWorkers();
}

size_t SimulatedNUMANode::getQueueDepth() const {
  return scheduler_->getApproximateWorkCount();
}

std::unordered_set<std::string> SimulatedNUMANode::getLocalAgents() const {
  return local_agents_;
}

void SimulatedNUMANode::resetStats() {
  local_steals_.store(0);
  remote_steals_.store(0);
  Logger::debug("SimulatedNUMANode {}: Stats reset", node_id_);
}

}  // namespace simulation
}  // namespace keystone
