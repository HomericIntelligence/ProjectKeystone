/**
 * @file test_logger.cpp
 * @brief Unit tests for Logger and LogContext
 */

#include "concurrency/logger.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace keystone::concurrency;

// Test: LogContext set and get
TEST(LogContextTest, SetAndGet) {
    LogContext::set("agent_1", 5, "session_abc");

    EXPECT_EQ(LogContext::getAgentId(), "agent_1");
    EXPECT_EQ(LogContext::getWorkerId(), 5);
    EXPECT_EQ(LogContext::getSessionId(), "session_abc");
}

// Test: LogContext clear
TEST(LogContextTest, Clear) {
    LogContext::set("agent_1", 5, "session_abc");
    LogContext::clear();

    EXPECT_EQ(LogContext::getAgentId(), "");
    EXPECT_EQ(LogContext::getWorkerId(), -1);
    EXPECT_EQ(LogContext::getSessionId(), "");
}

// Test: LogContext formatting
TEST(LogContextTest, ContextString) {
    LogContext::set("chief", 0, "sess_1");
    EXPECT_EQ(LogContext::getContextString(), "[chief:0:sess_1]");
}

// Test: LogContext with no context set
TEST(LogContextTest, NoContext) {
    LogContext::clear();
    EXPECT_EQ(LogContext::getContextString(), "[no-context]");
}

// Test: LogContext is thread-local
TEST(LogContextTest, ThreadLocal) {
    LogContext::clear();
    LogContext::set("main_thread", 0, "main_session");

    std::string other_thread_context;

    std::thread t([&other_thread_context]() {
        // In new thread, context should be empty
        other_thread_context = LogContext::getContextString();

        // Set different context in this thread
        LogContext::set("worker_thread", 1, "worker_session");
        EXPECT_EQ(LogContext::getAgentId(), "worker_thread");
    });

    t.join();

    // Main thread context should be unchanged
    EXPECT_EQ(LogContext::getAgentId(), "main_thread");
    EXPECT_EQ(other_thread_context, "[no-context]");
}

// Test: Logger initialization
TEST(LoggerTest, Initialization) {
    Logger::init(spdlog::level::info);
    // If we get here without crash, initialization succeeded
    SUCCEED();
    Logger::shutdown();
}

// Test: Logger basic logging (no crash test)
TEST(LoggerTest, BasicLogging) {
    Logger::init(spdlog::level::info);
    LogContext::set("test_agent", 0, "test_session");

    // These should not crash
    Logger::info("Test message");
    Logger::info("Test with arg: {}", 42);
    Logger::warn("Warning message");
    Logger::error("Error message");

    SUCCEED();
    Logger::shutdown();
}

// Test: Logger with different levels
TEST(LoggerTest, LogLevels) {
    Logger::init(spdlog::level::trace);
    LogContext::set("test_agent", 0, "test_session");

    // All levels should work
    Logger::trace("Trace message");
    Logger::debug("Debug message");
    Logger::info("Info message");
    Logger::warn("Warn message");
    Logger::error("Error message");
    Logger::critical("Critical message");

    SUCCEED();
    Logger::shutdown();
}

// Test: Logger set level
TEST(LoggerTest, SetLevel) {
    Logger::init(spdlog::level::info);

    // Change level
    Logger::setLevel(spdlog::level::debug);
    Logger::debug("Debug message after level change");

    SUCCEED();
    Logger::shutdown();
}

// Test: Logger multiple initializations
TEST(LoggerTest, MultipleInitializations) {
    Logger::init(spdlog::level::info);
    Logger::init(spdlog::level::debug);  // Should not crash

    Logger::info("After multiple inits");

    SUCCEED();
    Logger::shutdown();
}

// Test: Logger thread safety
TEST(LoggerTest, ThreadSafety) {
    Logger::init(spdlog::level::info);

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([i]() {
            LogContext::set("worker_" + std::to_string(i), i, "session_1");

            for (int j = 0; j < 100; ++j) {
                Logger::info("Thread {} message {}", i, j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    SUCCEED();
    Logger::shutdown();
}

// Test: Logger without initialization (auto-init)
TEST(LoggerTest, AutoInitialization) {
    // Logger should auto-initialize on first use
    Logger::info("Auto-initialized message");

    SUCCEED();
    Logger::shutdown();
}

// Test: LogContext with empty strings
TEST(LogContextTest, EmptyStrings) {
    LogContext::set("", 0, "");
    // Empty agent_id is treated as no context (better behavior)
    EXPECT_EQ(LogContext::getContextString(), "[no-context]");
}

// Test: LogContext with negative worker ID
TEST(LogContextTest, NegativeWorkerId) {
    LogContext::set("agent", -5, "session");
    EXPECT_EQ(LogContext::getWorkerId(), -5);
    EXPECT_EQ(LogContext::getContextString(), "[agent:-5:session]");
}
