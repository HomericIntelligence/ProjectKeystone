#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "simulation/simulated_numa_node.hpp"

using namespace keystone::simulation;

class SimulatedNUMANodeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Tests will create nodes as needed
  }
};

TEST_F(SimulatedNUMANodeTest, CreateAndDestroy) {
  SimulatedNUMANode node(0, 4);
  EXPECT_EQ(node.getNodeId(), 0);
  EXPECT_EQ(node.getNumWorkers(), 4);
}

TEST_F(SimulatedNUMANodeTest, StartAndShutdown) {
  SimulatedNUMANode node(0, 2);

  node.start();
  // Should be able to submit work after starting
  std::atomic<bool> executed{false};
  node.submit([&]() { executed = true; });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(executed);

  node.shutdown();
}

TEST_F(SimulatedNUMANodeTest, SubmitWork) {
  SimulatedNUMANode node(0, 4);
  node.start();

  std::atomic<int> counter{0};

  // Submit multiple work items
  for (int i = 0; i < 10; ++i) {
    node.submit([&]() { counter++; });
  }

  // Wait for completion
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_EQ(counter.load(), 10);
  node.shutdown();
}

TEST_F(SimulatedNUMANodeTest, SubmitToSpecificWorker) {
  SimulatedNUMANode node(0, 4);
  node.start();

  std::atomic<int> counter{0};

  // Submit to worker 0
  for (int i = 0; i < 5; ++i) {
    node.submitToWorker(0, [&]() { counter++; });
  }

  // Submit to worker 1
  for (int i = 0; i < 5; ++i) {
    node.submitToWorker(1, [&]() { counter++; });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_EQ(counter.load(), 10);

  node.shutdown();
}

TEST_F(SimulatedNUMANodeTest, AgentRegistration) {
  SimulatedNUMANode node(0, 4);

  EXPECT_FALSE(node.hasAgent("agent_A"));

  node.registerAgent("agent_A");
  EXPECT_TRUE(node.hasAgent("agent_A"));

  node.registerAgent("agent_B");
  EXPECT_TRUE(node.hasAgent("agent_B"));

  auto agents = node.getLocalAgents();
  EXPECT_EQ(agents.size(), 2);
  EXPECT_TRUE(agents.count("agent_A") > 0);
  EXPECT_TRUE(agents.count("agent_B") > 0);
}

TEST_F(SimulatedNUMANodeTest, AgentUnregistration) {
  SimulatedNUMANode node(0, 4);

  node.registerAgent("agent_A");
  node.registerAgent("agent_B");
  EXPECT_EQ(node.getLocalAgents().size(), 2);

  node.unregisterAgent("agent_A");
  EXPECT_FALSE(node.hasAgent("agent_A"));
  EXPECT_TRUE(node.hasAgent("agent_B"));
  EXPECT_EQ(node.getLocalAgents().size(), 1);

  node.unregisterAgent("agent_B");
  EXPECT_EQ(node.getLocalAgents().size(), 0);
}

TEST_F(SimulatedNUMANodeTest, LocalStealTracking) {
  SimulatedNUMANode node(0, 4);

  EXPECT_EQ(node.getLocalSteals(), 0);

  node.recordLocalSteal();
  EXPECT_EQ(node.getLocalSteals(), 1);

  node.recordLocalSteal();
  node.recordLocalSteal();
  EXPECT_EQ(node.getLocalSteals(), 3);
}

TEST_F(SimulatedNUMANodeTest, RemoteStealTracking) {
  SimulatedNUMANode node(0, 4);

  EXPECT_EQ(node.getRemoteSteals(), 0);

  // Attempt steal (will increment counter even if no work)
  node.stealWork();
  EXPECT_EQ(node.getRemoteSteals(), 1);

  node.stealWork();
  node.stealWork();
  EXPECT_EQ(node.getRemoteSteals(), 3);
}

TEST_F(SimulatedNUMANodeTest, QueueDepthTracking) {
  SimulatedNUMANode node(0, 4);
  node.start();

  // Initially empty
  EXPECT_EQ(node.getQueueDepth(), 0);

  // Submit work that blocks briefly
  for (int i = 0; i < 20; ++i) {
    node.submit(
        [&]() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); });
  }

  // Should have pending work
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  size_t depth = node.getQueueDepth();
  EXPECT_GT(depth, 0);

  // Wait for completion
  std::this_thread::sleep_for(std::chrono::seconds(2));
  EXPECT_EQ(node.getQueueDepth(), 0);

  node.shutdown();
}

TEST_F(SimulatedNUMANodeTest, ResetStats) {
  SimulatedNUMANode node(0, 4);

  node.recordLocalSteal();
  node.recordLocalSteal();
  node.stealWork();

  EXPECT_EQ(node.getLocalSteals(), 2);
  EXPECT_EQ(node.getRemoteSteals(), 1);

  node.resetStats();

  EXPECT_EQ(node.getLocalSteals(), 0);
  EXPECT_EQ(node.getRemoteSteals(), 0);
}

TEST_F(SimulatedNUMANodeTest, MultipleNodes) {
  SimulatedNUMANode node0(0, 2);
  SimulatedNUMANode node1(1, 2);
  SimulatedNUMANode node2(2, 2);

  EXPECT_EQ(node0.getNodeId(), 0);
  EXPECT_EQ(node1.getNodeId(), 1);
  EXPECT_EQ(node2.getNodeId(), 2);

  node0.start();
  node1.start();
  node2.start();

  std::atomic<int> counter0{0}, counter1{0}, counter2{0};

  node0.submit([&]() { counter0++; });
  node1.submit([&]() { counter1++; });
  node2.submit([&]() { counter2++; });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_EQ(counter0.load(), 1);
  EXPECT_EQ(counter1.load(), 1);
  EXPECT_EQ(counter2.load(), 1);

  node0.shutdown();
  node1.shutdown();
  node2.shutdown();
}
