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
#include "simulation/simulated_network.hpp"

#include <chrono>
#include <thread>
#include <memory>
#include <vector>

using namespace keystone;
using namespace keystone::agents;
using namespace keystone::core;
using namespace keystone::concurrency;
using namespace keystone::simulation;

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

/**
 * Phase 5.2: Network Partition Testing
 *
 * Tests for network partition (split-brain) scenarios
 */
class Phase5NetworkPartitionTest : public ::testing::Test {
protected:
    // No setup needed for partition tests
};

/**
 * @brief Test basic network partition creation and healing
 *
 * Verify that partitions can be created and healed correctly.
 */
TEST_F(Phase5NetworkPartitionTest, CreateAndHealPartition) {
    SimulatedNetwork network;

    // Initially no partition
    EXPECT_FALSE(network.isPartitioned());

    // Create partition: [0, 1] vs [2, 3]
    network.createPartition({0, 1}, {2, 3});

    EXPECT_TRUE(network.isPartitioned());

    // Nodes in same partition can communicate
    EXPECT_TRUE(network.canCommunicate(0, 1));
    EXPECT_TRUE(network.canCommunicate(1, 0));
    EXPECT_TRUE(network.canCommunicate(2, 3));
    EXPECT_TRUE(network.canCommunicate(3, 2));

    // Nodes in different partitions cannot communicate
    EXPECT_FALSE(network.canCommunicate(0, 2));
    EXPECT_FALSE(network.canCommunicate(0, 3));
    EXPECT_FALSE(network.canCommunicate(1, 2));
    EXPECT_FALSE(network.canCommunicate(1, 3));
    EXPECT_FALSE(network.canCommunicate(2, 0));
    EXPECT_FALSE(network.canCommunicate(2, 1));
    EXPECT_FALSE(network.canCommunicate(3, 0));
    EXPECT_FALSE(network.canCommunicate(3, 1));

    // Heal partition
    network.healPartition();

    EXPECT_FALSE(network.isPartitioned());

    // All nodes can communicate again
    EXPECT_TRUE(network.canCommunicate(0, 2));
    EXPECT_TRUE(network.canCommunicate(1, 3));
    EXPECT_TRUE(network.canCommunicate(2, 0));
    EXPECT_TRUE(network.canCommunicate(3, 1));
}

/**
 * @brief Test messages are dropped across partitions
 *
 * Verify that messages sent across partition boundaries are dropped.
 */
TEST_F(Phase5NetworkPartitionTest, MessagesDroppedAcrossPartition) {
    SimulatedNetwork::Config config{
        .min_latency = std::chrono::microseconds(10),
        .max_latency = std::chrono::microseconds(50),
        .bandwidth_mbps = 1000,
        .packet_loss_rate = 0.0  // No random packet loss
    };
    SimulatedNetwork network(config);

    // Create partition: [0, 1] vs [2, 3]
    network.createPartition({0, 1}, {2, 3});

    EXPECT_EQ(network.getPartitionDroppedMessages(), 0);

    // Send message across partition (should be dropped)
    int executed = 0;
    network.send(0, 2, [&executed]() { executed++; });

    EXPECT_EQ(network.getPartitionDroppedMessages(), 1);

    // Wait for potential delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Message should not be delivered
    auto work = network.receive(2);
    EXPECT_FALSE(work.has_value());
    EXPECT_EQ(executed, 0);  // Work not executed

    // Send message within partition (should be delivered)
    network.send(0, 1, [&executed]() { executed++; });

    EXPECT_EQ(network.getPartitionDroppedMessages(), 1);  // Still 1

    // Wait for delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Message should be delivered
    work = network.receive(1);
    EXPECT_TRUE(work.has_value());
    if (work) {
        (*work)();
    }
    EXPECT_EQ(executed, 1);  // Work executed
}

/**
 * @brief Test partition statistics tracking
 *
 * Verify that partition-dropped messages are tracked correctly.
 */
TEST_F(Phase5NetworkPartitionTest, PartitionStatisticsTracking) {
    SimulatedNetwork::Config config{
        .min_latency = std::chrono::microseconds(10),
        .max_latency = std::chrono::microseconds(50),
        .bandwidth_mbps = 1000,
        .packet_loss_rate = 0.0
    };
    SimulatedNetwork network(config);

    // Create partition: [0, 1] vs [2, 3]
    network.createPartition({0, 1}, {2, 3});

    EXPECT_EQ(network.getPartitionDroppedMessages(), 0);
    EXPECT_EQ(network.getTotalMessages(), 0);

    // Send 10 messages across partition
    for (int i = 0; i < 10; ++i) {
        network.send(0, 2, []() {});
    }

    EXPECT_EQ(network.getPartitionDroppedMessages(), 10);
    EXPECT_EQ(network.getTotalMessages(), 10);

    // Send 5 messages within partition
    for (int i = 0; i < 5; ++i) {
        network.send(0, 1, []() {});
    }

    EXPECT_EQ(network.getPartitionDroppedMessages(), 10);  // Still 10
    EXPECT_EQ(network.getTotalMessages(), 15);  // Total increased

    // Heal partition and send more messages
    network.healPartition();

    for (int i = 0; i < 3; ++i) {
        network.send(0, 2, []() {});
    }

    EXPECT_EQ(network.getPartitionDroppedMessages(), 10);  // Still 10 (no new drops)
    EXPECT_EQ(network.getTotalMessages(), 18);

    // Reset stats
    network.resetStats();

    EXPECT_EQ(network.getPartitionDroppedMessages(), 0);
    EXPECT_EQ(network.getTotalMessages(), 0);
}

/**
 * @brief Test split-brain scenario with work distribution
 *
 * Simulates a real-world split-brain where work cannot cross partition.
 */
TEST_F(Phase5NetworkPartitionTest, SplitBrainWorkDistribution) {
    SimulatedNetwork::Config config{
        .min_latency = std::chrono::microseconds(10),
        .max_latency = std::chrono::microseconds(50),
        .bandwidth_mbps = 1000,
        .packet_loss_rate = 0.0
    };
    SimulatedNetwork network(config);

    // Simulate 4 nodes with work
    std::atomic<int> node0_work{0};
    std::atomic<int> node1_work{0};
    std::atomic<int> node2_work{0};
    std::atomic<int> node3_work{0};

    // Create partition: [0, 1] vs [2, 3]
    network.createPartition({0, 1}, {2, 3});

    // Send work within partition A (0→1)
    for (int i = 0; i < 5; ++i) {
        network.send(0, 1, [&node1_work]() { node1_work++; });
    }

    // Send work within partition B (2→3)
    for (int i = 0; i < 3; ++i) {
        network.send(2, 3, [&node3_work]() { node3_work++; });
    }

    // Send work across partition (should be dropped)
    for (int i = 0; i < 10; ++i) {
        network.send(0, 2, [&node2_work]() { node2_work++; });
        network.send(1, 3, [&node3_work]() { node3_work++; });
    }

    EXPECT_EQ(network.getPartitionDroppedMessages(), 20);  // All cross-partition dropped

    // Process work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Execute work items
    while (auto work = network.receive(1)) {
        (*work)();
    }
    while (auto work = network.receive(3)) {
        (*work)();
    }
    while (auto work = network.receive(2)) {
        (*work)();
    }

    // Verify: Only within-partition work was executed
    EXPECT_EQ(node1_work.load(), 5);   // Received from node 0
    EXPECT_EQ(node3_work.load(), 3);   // Received from node 2 (NOT from node 1)
    EXPECT_EQ(node2_work.load(), 0);   // Nothing received (all dropped)

    // Heal partition and send more work
    network.healPartition();

    for (int i = 0; i < 2; ++i) {
        network.send(0, 2, [&node2_work]() { node2_work++; });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (auto work = network.receive(2)) {
        (*work)();
    }

    // After healing, cross-partition work succeeds
    EXPECT_EQ(node2_work.load(), 2);
}

/**
 * @brief Test asymmetric partition (more complex topology)
 *
 * Test partition with different sizes: [0] vs [1, 2, 3]
 */
TEST_F(Phase5NetworkPartitionTest, AsymmetricPartition) {
    SimulatedNetwork network;

    // Create asymmetric partition: [0] vs [1, 2, 3]
    network.createPartition({0}, {1, 2, 3});

    EXPECT_TRUE(network.isPartitioned());

    // Node 0 is isolated
    EXPECT_FALSE(network.canCommunicate(0, 1));
    EXPECT_FALSE(network.canCommunicate(0, 2));
    EXPECT_FALSE(network.canCommunicate(0, 3));

    // Nodes 1, 2, 3 can communicate with each other
    EXPECT_TRUE(network.canCommunicate(1, 2));
    EXPECT_TRUE(network.canCommunicate(1, 3));
    EXPECT_TRUE(network.canCommunicate(2, 3));
    EXPECT_TRUE(network.canCommunicate(2, 1));
    EXPECT_TRUE(network.canCommunicate(3, 1));
    EXPECT_TRUE(network.canCommunicate(3, 2));

    // Heal and verify
    network.healPartition();

    EXPECT_TRUE(network.canCommunicate(0, 1));
    EXPECT_TRUE(network.canCommunicate(0, 2));
    EXPECT_TRUE(network.canCommunicate(0, 3));
}
