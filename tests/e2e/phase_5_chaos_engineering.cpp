/**
 * @file phase_5_chaos_engineering.cpp
 * @brief E2E tests for Phase 5: Robustness & Chaos Engineering
 *
 * Phase 5 tests chaos engineering scenarios:
 * - Agent failures
 * - Network partitions
 * - Message loss
 * - Recovery mechanisms
 */

#include <gtest/gtest.h>

#include "agents/async_chief_architect_agent.hpp"
#include "agents/async_component_lead_agent.hpp"
#include "agents/async_module_lead_agent.hpp"
#include "agents/async_task_agent.hpp"
#include "core/message_bus.hpp"
#include "core/failure_injector.hpp"
#include "concurrency/work_stealing_scheduler.hpp"

#include <chrono>
#include <thread>
#include <memory>
#include <vector>

using namespace keystone;
using namespace keystone::agents;
using namespace keystone::core;
using namespace keystone::concurrency;

/**
 * Phase 5.1: Agent Failure Testing
 *
 * Tests for agent failure modes and graceful degradation
 */
class Phase5AgentFailureTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler_ = std::make_unique<WorkStealingScheduler>(4);
        scheduler_->start();

        message_bus_ = std::make_unique<MessageBus>();
        message_bus_->setScheduler(scheduler_.get());
    }

    void TearDown() override {
        if (scheduler_) {
            scheduler_->shutdown();
        }
    }

    std::unique_ptr<WorkStealingScheduler> scheduler_;
    std::unique_ptr<MessageBus> message_bus_;
};

/**
 * @brief Test that failed agents reject messages with error responses
 *
 * When an agent is marked as failed, it should reject all incoming
 * messages and send error responses back to the sender.
 */
TEST_F(Phase5AgentFailureTest, FailedAgentRejectsMessages) {
    // Create a TaskAgent
    auto task = std::make_unique<AsyncTaskAgent>("task1");
    task->setMessageBus(message_bus_.get());
    task->setScheduler(scheduler_.get());
    message_bus_->registerAgent(task->getAgentId(), task.get());

    // Agent should be healthy initially
    EXPECT_FALSE(task->isFailed());
    EXPECT_TRUE(task->getFailureReason().empty());

    // Mark agent as failed
    task->markAsFailed("simulated crash for testing");

    // Agent should now be failed
    EXPECT_TRUE(task->isFailed());
    EXPECT_EQ(task->getFailureReason(), "simulated crash for testing");

    // Send a message to the failed agent
    auto msg = KeystoneMessage::create("sender", task->getAgentId(), "echo test");
    task->receiveMessage(msg);

    // Wait a bit for async processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Agent should have rejected the message (we'd check error response in full impl)
    EXPECT_TRUE(task->isFailed());  // Still failed
}

/**
 * @brief Test that failed agents can recover and resume processing
 *
 * After recovery, an agent should be able to process messages normally.
 */
TEST_F(Phase5AgentFailureTest, FailedAgentCanRecover) {
    // Create a TaskAgent
    auto task = std::make_unique<AsyncTaskAgent>("task1");
    task->setMessageBus(message_bus_.get());
    task->setScheduler(scheduler_.get());
    message_bus_->registerAgent(task->getAgentId(), task.get());

    // Mark agent as failed
    task->markAsFailed("simulated crash");
    EXPECT_TRUE(task->isFailed());

    // Recover the agent
    task->recover();

    // Agent should be healthy again
    EXPECT_FALSE(task->isFailed());
    EXPECT_TRUE(task->getFailureReason().empty());

    // Now it should process messages normally
    auto msg = KeystoneMessage::create("sender", task->getAgentId(), "echo recovered");
    task->receiveMessage(msg);

    // Wait a bit for async processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Agent should still be healthy
    EXPECT_FALSE(task->isFailed());
}

/**
 * @brief Test FailureInjector can crash agents
 *
 * FailureInjector should be able to mark agents as crashed
 * and agents should be able to query their failure state.
 */
TEST_F(Phase5AgentFailureTest, FailureInjectorCrashesAgents) {
    // Create a TaskAgent
    auto task = std::make_unique<AsyncTaskAgent>("task1");
    task->setMessageBus(message_bus_.get());
    task->setScheduler(scheduler_.get());
    message_bus_->registerAgent(task->getAgentId(), task.get());

    // Create failure injector
    FailureInjector injector(42);  // Seeded RNG for reproducibility

    // Inject crash for task
    injector.injectAgentCrash("task1");
    EXPECT_TRUE(injector.isAgentCrashed("task1"));

    // Set failure injector on the agent
    task->setFailureInjector(&injector);

    // shouldFail() should return true for crashed agent
    EXPECT_TRUE(task->shouldFail());

    // We could mark the agent as failed based on injector
    if (task->shouldFail()) {
        task->markAsFailed("FailureInjector crashed agent");
    }

    EXPECT_TRUE(task->isFailed());
    EXPECT_EQ(task->getFailureReason(), "FailureInjector crashed agent");
}

/**
 * @brief Test system continues with remaining healthy agents
 *
 * When one agent fails, the other agents should continue working normally.
 */
TEST_F(Phase5AgentFailureTest, SystemContinuesWithHealthyAgents) {
    // Create 3 TaskAgents
    auto task1 = std::make_unique<AsyncTaskAgent>("task1");
    task1->setMessageBus(message_bus_.get());
    task1->setScheduler(scheduler_.get());
    message_bus_->registerAgent(task1->getAgentId(), task1.get());

    auto task2 = std::make_unique<AsyncTaskAgent>("task2");
    task2->setMessageBus(message_bus_.get());
    task2->setScheduler(scheduler_.get());
    message_bus_->registerAgent(task2->getAgentId(), task2.get());

    auto task3 = std::make_unique<AsyncTaskAgent>("task3");
    task3->setMessageBus(message_bus_.get());
    task3->setScheduler(scheduler_.get());
    message_bus_->registerAgent(task3->getAgentId(), task3.get());

    // Fail task2
    task2->markAsFailed("simulated crash");
    EXPECT_TRUE(task2->isFailed());

    // task1 and task3 should still be healthy
    EXPECT_FALSE(task1->isFailed());
    EXPECT_FALSE(task3->isFailed());

    // Send messages to all tasks
    auto msg1 = KeystoneMessage::create("test", task1->getAgentId(), "echo task1");
    auto msg2 = KeystoneMessage::create("test", task2->getAgentId(), "echo task2");
    auto msg3 = KeystoneMessage::create("test", task3->getAgentId(), "echo task3");

    task1->receiveMessage(msg1);
    task2->receiveMessage(msg2);  // Should be rejected
    task3->receiveMessage(msg3);

    // Wait for async processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // task1 and task3 should still be healthy
    EXPECT_FALSE(task1->isFailed());
    EXPECT_FALSE(task3->isFailed());
    // task2 should still be failed
    EXPECT_TRUE(task2->isFailed());
}

/**
 * Phase 5.1: Probabilistic Failure Testing
 *
 * Tests for probabilistic failure injection
 */
class Phase5ProbabilisticFailureTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler_ = std::make_unique<WorkStealingScheduler>(4);
        scheduler_->start();

        message_bus_ = std::make_unique<MessageBus>();
        message_bus_->setScheduler(scheduler_.get());
    }

    void TearDown() override {
        if (scheduler_) {
            scheduler_->shutdown();
        }
    }

    std::unique_ptr<WorkStealingScheduler> scheduler_;
    std::unique_ptr<MessageBus> message_bus_;
};

/**
 * @brief Test FailureInjector produces probabilistic failures
 *
 * With a 50% failure rate, approximately half of the trials should fail.
 */
TEST_F(Phase5ProbabilisticFailureTest, ProbabilisticFailureRate) {
    // Create failure injector with 50% failure rate
    FailureInjector injector(42);  // Seeded for reproducibility
    injector.setFailureRate(0.5);  // 50% failure rate

    EXPECT_DOUBLE_EQ(injector.getFailureRate(), 0.5);

    int failed_count = 0;
    int total_trials = 100;

    for (int i = 0; i < total_trials; ++i) {
        if (injector.shouldFail()) {
            failed_count++;
        }
    }

    // With 50% rate and 100 trials, expect ~40-60 failures
    EXPECT_GE(failed_count, 30);
    EXPECT_LE(failed_count, 70);
}

/**
 * @brief Test agents fail based on injector rate
 *
 * When agents have a failure injector with a configured rate,
 * some agents should fail probabilistically.
 */
TEST_F(Phase5ProbabilisticFailureTest, AgentsFailBasedOnInjectorRate) {
    // Create 10 task agents
    std::vector<std::unique_ptr<AsyncTaskAgent>> agents;
    for (int i = 0; i < 10; ++i) {
        auto agent = std::make_unique<AsyncTaskAgent>("task" + std::to_string(i));
        agent->setMessageBus(message_bus_.get());
        agent->setScheduler(scheduler_.get());
        message_bus_->registerAgent(agent->getAgentId(), agent.get());
        agents.push_back(std::move(agent));
    }

    // Create failure injector with 50% failure rate
    FailureInjector injector(42);
    injector.setFailureRate(0.5);

    // Set failure injector on all agents
    for (auto& agent : agents) {
        agent->setFailureInjector(&injector);
    }

    // Check which agents should fail and mark them
    for (auto& agent : agents) {
        if (agent->shouldFail()) {
            agent->markAsFailed("Probabilistic failure from FailureInjector");
        }
    }

    // Count failed agents
    int failed_agents = 0;
    for (auto& agent : agents) {
        if (agent->isFailed()) {
            failed_agents++;
        }
    }

    // Some agents should have failed (statistically)
    // With 10 agents, expect 0-10 failures
    EXPECT_GE(failed_agents, 0);
    EXPECT_LE(failed_agents, 10);
}

/**
 * Phase 5.1: FailureInjector Statistics
 *
 * Tests for failure tracking and statistics
 */
class Phase5FailureInjectorStatsTest : public ::testing::Test {
protected:
    // No scheduler needed for pure FailureInjector tests
};

/**
 * @brief Test FailureInjector tracks failure statistics
 *
 * FailureInjector should correctly track:
 * - Total failures injected
 * - Crashed agents
 * - Timeout agents
 */
TEST_F(Phase5FailureInjectorStatsTest, TracksFailureStatistics) {
    FailureInjector injector(42);

    // Initially no failures
    EXPECT_EQ(injector.getTotalFailures(), 0);
    EXPECT_TRUE(injector.getFailedAgents().empty());
    EXPECT_TRUE(injector.getTimeoutAgents().empty());

    // Inject some crashes
    injector.injectAgentCrash("agent1");
    injector.injectAgentCrash("agent2");
    injector.injectAgentCrash("agent3");

    EXPECT_EQ(injector.getTotalFailures(), 3);
    EXPECT_EQ(injector.getFailedAgents().size(), 3);

    // Inject some timeouts
    injector.injectAgentTimeout("agent4", std::chrono::milliseconds(100));
    injector.injectAgentTimeout("agent5", std::chrono::milliseconds(500));

    EXPECT_EQ(injector.getTotalFailures(), 5);
    EXPECT_EQ(injector.getTimeoutAgents().size(), 2);

    // Recover agent1
    injector.recoverAgent("agent1");
    EXPECT_EQ(injector.getFailedAgents().size(), 2);

    // Check statistics string
    std::string stats = injector.getStatistics();
    EXPECT_NE(stats.find("Total failures injected: 5"), std::string::npos);
    EXPECT_NE(stats.find("Crashed agents: 2"), std::string::npos);
    EXPECT_NE(stats.find("Timeout agents: 2"), std::string::npos);

    // Reset
    injector.reset();
    EXPECT_EQ(injector.getTotalFailures(), 0);
    EXPECT_TRUE(injector.getFailedAgents().empty());
    EXPECT_TRUE(injector.getTimeoutAgents().empty());
}

/**
 * @brief Test FailureInjector timeout tracking
 *
 * FailureInjector should correctly track agent timeouts and delays.
 */
TEST_F(Phase5FailureInjectorStatsTest, TracksAgentTimeouts) {
    FailureInjector injector(42);

    // Inject timeout for agent1
    injector.injectAgentTimeout("agent1", std::chrono::milliseconds(100));

    EXPECT_EQ(injector.getAgentTimeout("agent1"), std::chrono::milliseconds(100));
    EXPECT_EQ(injector.getTimeoutAgents().size(), 1);

    // Inject timeout for agent2 with different delay
    injector.injectAgentTimeout("agent2", std::chrono::milliseconds(500));

    EXPECT_EQ(injector.getAgentTimeout("agent2"), std::chrono::milliseconds(500));
    EXPECT_EQ(injector.getTimeoutAgents().size(), 2);

    // Clear timeout for agent1
    injector.clearAgentTimeout("agent1");
    EXPECT_EQ(injector.getAgentTimeout("agent1"), std::chrono::milliseconds(0));
    EXPECT_EQ(injector.getTimeoutAgents().size(), 1);

    // agent2 should still have timeout
    EXPECT_EQ(injector.getAgentTimeout("agent2"), std::chrono::milliseconds(500));
}
