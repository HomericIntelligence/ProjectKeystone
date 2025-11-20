/**
 * @file logger.cpp
 * @brief Implementation of Logger and LogContext
 */

#include "concurrency/logger.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <sstream>

namespace keystone {
namespace concurrency {

// LogContext thread-local storage
thread_local LogContext::Context LogContext::context_;

void LogContext::set(const std::string& agent_id, int worker_id,
                     const std::string& session_id) {
  context_.agent_id = agent_id;
  context_.worker_id = worker_id;
  context_.session_id = session_id;
}

void LogContext::clear() {
  context_.agent_id.clear();
  context_.worker_id = -1;
  context_.session_id.clear();
}

std::string LogContext::getAgentId() { return context_.agent_id; }

int LogContext::getWorkerId() { return context_.worker_id; }

std::string LogContext::getSessionId() { return context_.session_id; }

std::string LogContext::getContextString() {
  if (context_.agent_id.empty()) {
    return "[no-context]";
  }

  std::ostringstream oss;
  oss << "[" << context_.agent_id << ":" << context_.worker_id << ":"
      << context_.session_id << "]";
  return oss.str();
}

// Logger static members
std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::init(spdlog::level::level_enum level) {
  if (!logger_) {
    logger_ = spdlog::stdout_color_mt("keystone");
    logger_->set_level(level);
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
  }
}

void Logger::shutdown() {
  if (logger_) {
    spdlog::drop("keystone");
    logger_.reset();
  }
}

void Logger::setLevel(spdlog::level::level_enum level) {
  if (logger_) {
    logger_->set_level(level);
  }
}

}  // namespace concurrency
}  // namespace keystone
