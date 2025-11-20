/**
 * @file test_message_bus_async.cpp
 * @brief Tests for MessageBus with WorkStealingScheduler integration
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "agents/base_agent.hpp"
#include "concurrency/work_stealing_scheduler.hpp"
#include "core/message_bus.hpp"

using namespace keystone::core;
using namespace keystone::agents;
using namespace keystone::concurrency;

// Simple test agent that counts received messages
class CountingAgent : public BaseAgent {
 public:
  explicit CountingAgent(const std::string& agent_id)
      : BaseAgent(agent_id), count_(std::make_shared<std::atomic<int>>(0)) {}

  Response processMessage(const KeystoneMessage& msg) override {
    count_->fetch_add(1);
    Response resp;
    resp.msg_id = msg.msg_id;
    resp.sender_id = agent_id_;
    resp.receiver_id = msg.sender_id;
    resp.status = Response::Status::Success;
    resp.result = "OK";
    resp.timestamp = std::chrono::system_clock::now();
    return resp;
  }

  int getCount() const { return count_->load(); }

 private:
  std::shared_ptr<std::atomic<int>> count_;
};

// Test: MessageBus without scheduler (synchronous routing)
TEST(MessageBusAsyncTest, SynchronousRoutingWithoutScheduler) {
  MessageBus bus;
  CountingAgent agent1("agent1");
  CountingAgent agent2("agent2");

  bus.registerAgent("agent1", &agent1);
  bus.registerAgent("agent2", &agent2);

  agent1.setMessageBus(&bus);
  agent2.setMessageBus(&bus);

  // No scheduler set - should use synchronous routing
  EXPECT_EQ(bus.getScheduler(), nullptr);

  // Send messages
  auto msg1 = KeystoneMessage::create("agent1", "agent2", "test1");
  auto msg2 = KeystoneMessage::create("agent1", "agent2", "test2");

  bus.routeMessage(msg1);
  bus.routeMessage(msg2);

  // Messages delivered synchronously - inbox should have them immediately
  auto received1 = agent2.getMessage();
  auto received2 = agent2.getMessage();

  EXPECT_TRUE(received1.has_value());
  EXPECT_TRUE(received2.has_value());
  EXPECT_EQ(received1->command, "test1");
  EXPECT_EQ(received2->command, "test2");
}

// Test: MessageBus with scheduler (async routing)
TEST(MessageBusAsyncTest, AsyncRoutingWithScheduler) {
  WorkStealingScheduler scheduler(2);
  scheduler.start();

  MessageBus bus;
  bus.setScheduler(&scheduler);

  CountingAgent agent1("agent1");
  CountingAgent agent2("agent2");

  bus.registerAgent("agent1", &agent1);
  bus.registerAgent("agent2", &agent2);

  agent1.setMessageBus(&bus);
  agent2.setMessageBus(&bus);

  // Scheduler should be set
  EXPECT_EQ(bus.getScheduler(), &scheduler);

  // Send messages - they will be delivered asynchronously
  for (int i = 0; i < 10; ++i) {
    auto msg = KeystoneMessage::create("agent1", "agent2",
                                       "test_" + std::to_string(i));
    bus.routeMessage(msg);
  }

  // Wait for async delivery
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Check that messages were received
  int received_count = 0;
  while (auto msg = agent2.getMessage()) {
    received_count++;
  }

  EXPECT_EQ(received_count, 10);

  scheduler.shutdown();
}

// Test: Multiple agents with scheduler
TEST(MessageBusAsyncTest, MultipleAgentsAsyncRouting) {
  WorkStealingScheduler scheduler(4);
  scheduler.start();

  MessageBus bus;
  bus.setScheduler(&scheduler);

  CountingAgent agent1("agent1");
  CountingAgent agent2("agent2");
  CountingAgent agent3("agent3");

  bus.registerAgent("agent1", &agent1);
  bus.registerAgent("agent2", &agent2);
  bus.registerAgent("agent3", &agent3);

  agent1.setMessageBus(&bus);
  agent2.setMessageBus(&bus);
  agent3.setMessageBus(&bus);

  // Send messages to multiple agents
  for (int i = 0; i < 20; ++i) {
    auto msg2 =
        KeystoneMessage::create("agent1", "agent2", "msg" + std::to_string(i));
    auto msg3 =
        KeystoneMessage::create("agent1", "agent3", "msg" + std::to_string(i));
    bus.routeMessage(msg2);
    bus.routeMessage(msg3);
  }

  // Wait for async delivery
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Check both agents received messages
  int count2 = 0, count3 = 0;
  while (agent2.getMessage()) count2++;
  while (agent3.getMessage()) count3++;

  EXPECT_EQ(count2, 20);
  EXPECT_EQ(count3, 20);

  scheduler.shutdown();
}

// Test: Switching from sync to async
TEST(MessageBusAsyncTest, SwitchFromSyncToAsync) {
  MessageBus bus;
  CountingAgent agent1("agent1");
  CountingAgent agent2("agent2");

  bus.registerAgent("agent1", &agent1);
  bus.registerAgent("agent2", &agent2);

  agent1.setMessageBus(&bus);
  agent2.setMessageBus(&bus);

  // Start with synchronous routing
  EXPECT_EQ(bus.getScheduler(), nullptr);

  auto msg1 = KeystoneMessage::create("agent1", "agent2", "sync_msg");
  bus.routeMessage(msg1);

  // Should be delivered immediately
  EXPECT_TRUE(agent2.getMessage().has_value());

  // Now enable async routing
  WorkStealingScheduler scheduler(2);
  scheduler.start();
  bus.setScheduler(&scheduler);

  auto msg2 = KeystoneMessage::create("agent1", "agent2", "async_msg");
  bus.routeMessage(msg2);

  // Wait for async delivery
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  EXPECT_TRUE(agent2.getMessage().has_value());

  scheduler.shutdown();
}

// Test: High-load async routing
TEST(MessageBusAsyncTest, HighLoadAsyncRouting) {
  WorkStealingScheduler scheduler(4);
  scheduler.start();

  MessageBus bus;
  bus.setScheduler(&scheduler);

  CountingAgent agent1("agent1");
  CountingAgent agent2("agent2");

  bus.registerAgent("agent1", &agent1);
  bus.registerAgent("agent2", &agent2);

  agent1.setMessageBus(&bus);
  agent2.setMessageBus(&bus);

  // Send many messages
  const int num_messages = 1000;
  for (int i = 0; i < num_messages; ++i) {
    auto msg =
        KeystoneMessage::create("agent1", "agent2", "msg" + std::to_string(i));
    bus.routeMessage(msg);
  }

  // Wait for async delivery
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // Count received messages
  int received = 0;
  while (agent2.getMessage()) {
    received++;
  }

  EXPECT_EQ(received, num_messages);

  scheduler.shutdown();
}

// Test: Scheduler shutdown doesn't affect MessageBus
TEST(MessageBusAsyncTest, SchedulerShutdownGraceful) {
  WorkStealingScheduler scheduler(2);
  scheduler.start();

  MessageBus bus;
  bus.setScheduler(&scheduler);

  CountingAgent agent1("agent1");
  CountingAgent agent2("agent2");

  bus.registerAgent("agent1", &agent1);
  bus.registerAgent("agent2", &agent2);

  agent1.setMessageBus(&bus);
  agent2.setMessageBus(&bus);

  // Send messages
  for (int i = 0; i < 5; ++i) {
    auto msg =
        KeystoneMessage::create("agent1", "agent2", "msg" + std::to_string(i));
    bus.routeMessage(msg);
  }

  // Shutdown scheduler (waits for pending work)
  scheduler.shutdown();

  // All messages should have been delivered before shutdown completed
  int received = 0;
  while (agent2.getMessage()) {
    received++;
  }

  EXPECT_EQ(received, 5);
}
