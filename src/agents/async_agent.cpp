#include "agents/async_agent.hpp"

namespace keystone {
namespace agents {

AsyncAgent::AsyncAgent(const std::string& agent_id) : AgentCore(agent_id) {}

core::Response AsyncAgent::handleCancellation(const core::KeystoneMessage& msg) {
  // Extract task_id from the cancellation message
  if (!msg.task_id.has_value()) {
    return core::Response::createError(msg, agent_id_,
                                       "CANCEL_TASK message missing task_id");
  }

  const std::string& task_id = *msg.task_id;

  // Mark the task as cancelled
  requestCancellation(task_id);

  // Return acknowledgement
  return core::Response::createSuccess(msg, agent_id_,
                                       "Task " + task_id + " marked for cancellation");
}

}  // namespace agents
}  // namespace keystone
