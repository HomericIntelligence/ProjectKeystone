#pragma once

#include <memory>
#include <string>
#include <thread>

#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace keystone {
namespace concurrency {

/**
 * @brief LogContext - Thread-local context for distributed logging
 *
 * Provides thread-local context information (agent_id, worker_id, session_id)
 * that is automatically included in all log messages from that thread.
 *
 * Usage:
 *   LogContext::set("agent_123", 5, "session_abc");
 *   Logger::info("Processing message");  // Automatically includes context
 */
class LogContext {
 public:
  /**
   * @brief Set the thread-local logging context
   *
   * @param agent_id ID of the agent in this thread
   * @param worker_id Worker thread index
   * @param session_id Session identifier
   */
  static void set(const std::string& agent_id, int32_t worker_id,
                  const std::string& session_id);

  /**
   * @brief Clear the thread-local logging context
   */
  static void clear();

  /**
   * @brief Get current agent ID
   */
  static std::string getAgentId();

  /**
   * @brief Get current worker ID
   */
  static int32_t getWorkerId();

  /**
   * @brief Get current session ID
   */
  static std::string getSessionId();

  /**
   * @brief Get formatted context string for logging
   *
   * Returns format: "[agent_id:worker_id:session_id]"
   */
  static std::string getContextString();

 private:
  struct Context {
    std::string agent_id;
    int32_t worker_id = -1;
    std::string session_id;
  };

  static thread_local Context context_;
};

/**
 * @brief Logger - Wrapper around spdlog with automatic context injection
 *
 * Provides structured logging with automatic thread-local context.
 * All log messages include [agent_id:worker_id:session_id] prefix.
 *
 * Features:
 * - Multiple log levels (trace, debug, info, warn, error, critical)
 * - Automatic context from LogContext
 * - Thread-safe
 * - Colored console output
 *
 * Usage:
 *   Logger::init();  // One-time initialization
 *   LogContext::set("chief", 0, "session_1");
 *   Logger::info("Delegating to {}", "task_agent");
 *   // Output: [2025-11-18 12:34:56.789] [info] [chief:0:session_1] Delegating
 * to task_agent
 */
class Logger {
 public:
  /**
   * @brief Initialize the logger (call once at startup)
   *
   * @param level Log level (default: info)
   */
  static void init(spdlog::level::level_enum level = spdlog::level::info);

  /**
   * @brief Shutdown the logger (call at exit)
   */
  static void shutdown();

  /**
   * @brief Set the global log level
   */
  static void setLevel(spdlog::level::level_enum level);

  // Logging methods with automatic context injection
  template <typename... Args>
  static void trace(const std::string& fmt, Args&&... args) {
    log(spdlog::level::trace, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void debug(const std::string& fmt, Args&&... args) {
    log(spdlog::level::debug, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void info(const std::string& fmt, Args&&... args) {
    log(spdlog::level::info, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void warn(const std::string& fmt, Args&&... args) {
    log(spdlog::level::warn, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void error(const std::string& fmt, Args&&... args) {
    log(spdlog::level::err, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void critical(const std::string& fmt, Args&&... args) {
    log(spdlog::level::critical, fmt, std::forward<Args>(args)...);
  }

 private:
  static std::shared_ptr<spdlog::logger> logger_;

  template <typename... Args>
  static void log(spdlog::level::level_enum level, const std::string& fmt, Args&&... args) {
    if (!logger_) {
      init();
    }

    // Inject context into message
    std::string context = LogContext::getContextString();
    std::string full_fmt = context + " " + fmt;

    // Use runtime format to avoid compile-time format string requirement
    logger_->log(spdlog::source_loc{}, level, fmt::runtime(full_fmt), std::forward<Args>(args)...);
  }
};

}  // namespace concurrency
}  // namespace keystone
