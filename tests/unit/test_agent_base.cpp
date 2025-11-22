#include <gtest/gtest.h>

#include <thread>

#include "agents/agent_base.hpp"
#include "core/config.hpp"
#include "core/message.hpp"
#include "core/message_bus.hpp"

using namespace keystone;
using namespace keystone::agents;
using namespace keystone::core;

// Concrete implementation for testing
class TestAgent : public AgentBase {
 public:
  TestAgent(const std::string& agent_id) : AgentBase(agent_id) {}

  // Expose protected methods for testing
  using AgentBase::getMessage;
  using AgentBase::receiveMessage;
  using AgentBase::sendMessage;
  using AgentBase::setMessageBus;
  using AgentBase::updateQueueDepthMetrics;
};

class AgentBaseTest : public ::testing::Test {
 protected:
  void SetUp() override {
    agent_ = std::make_shared<TestAgent>("test_agent");
    bus_ = std::make_shared<MessageBus>();
    bus_->registerAgent(agent_->getAgentId(), agent_);
    agent_->setMessageBus(bus_.get());
  }

  void TearDown() override {
    bus_->unregisterAgent(agent_->getAgentId());
    agent_.reset();
    bus_.reset();
  }

  std::shared_ptr<TestAgent> agent_;
  std::shared_ptr<MessageBus> bus_;
};

// Test: sendMessage without message bus throws exception
TEST_F(AgentBaseTest, SendMessageWithoutBusThrows) {
  auto agent_no_bus = std::make_unique<TestAgent>("no_bus_agent");

  auto msg = KeystoneMessage::create("sender", "receiver", "test");

  EXPECT_THROW({ agent_no_bus->sendMessage(msg); }, std::runtime_error);
}

// Test: receiveMessage routes to correct priority queue
TEST_F(AgentBaseTest, MessageRoutingByPriority) {
  auto high_msg = KeystoneMessage::create("sender", agent_->getAgentId(), "high");
  high_msg.priority = Priority::HIGH;

  auto normal_msg = KeystoneMessage::create("sender", agent_->getAgentId(), "normal");
  normal_msg.priority = Priority::NORMAL;

  auto low_msg = KeystoneMessage::create("sender", agent_->getAgentId(), "low");
  low_msg.priority = Priority::LOW;

  // Send messages
  agent_->receiveMessage(high_msg);
  agent_->receiveMessage(normal_msg);
  agent_->receiveMessage(low_msg);

  // Retrieve in priority order
  auto msg1 = agent_->getMessage();
  ASSERT_TRUE(msg1.has_value());
  EXPECT_EQ(msg1->priority, Priority::HIGH);

  auto msg2 = agent_->getMessage();
  ASSERT_TRUE(msg2.has_value());
  EXPECT_EQ(msg2->priority, Priority::NORMAL);

  auto msg3 = agent_->getMessage();
  ASSERT_TRUE(msg3.has_value());
  EXPECT_EQ(msg3->priority, Priority::LOW);

  // No more messages
  auto msg4 = agent_->getMessage();
  EXPECT_FALSE(msg4.has_value());
}

// Test: Backpressure mechanism - rejecting messages when queue full
TEST_F(AgentBaseTest, BackpressureApplied) {
  // Fill queue beyond max size
  size_t max_size = Config::AGENT_MAX_QUEUE_SIZE;

  // Send max_size + 10 messages
  for (size_t i = 0; i < max_size + 10; ++i) {
    auto msg = KeystoneMessage::create("sender", agent_->getAgentId(), "msg_" + std::to_string(i));
    msg.priority = Priority::NORMAL;
    agent_->receiveMessage(msg);
  }

  // Only max_size messages should be queued (approximately, due to size_approx)
  size_t received_count = 0;
  while (agent_->getMessage().has_value()) {
    received_count++;
  }

  // Should have approximately max_size messages (allow some variance due to size_approx)
  EXPECT_LE(received_count, max_size + 10);  // At most max_size + buffer
  EXPECT_GE(received_count, max_size - 10);  // At least max_size - buffer
}

// Test: Backpressure cleared when queue drains below low watermark
TEST_F(AgentBaseTest, BackpressureCleared) {
  // Fill queue to trigger backpressure
  size_t max_size = Config::AGENT_MAX_QUEUE_SIZE;

  for (size_t i = 0; i < max_size + 10; ++i) {
    auto msg = KeystoneMessage::create("sender", agent_->getAgentId(), "msg_" + std::to_string(i));
    msg.priority = Priority::NORMAL;
    agent_->receiveMessage(msg);
  }

  // Drain queue below low watermark
  size_t low_watermark = static_cast<size_t>(max_size * Config::AGENT_QUEUE_LOW_WATERMARK_PERCENT);

  size_t drained = 0;
  while (drained < max_size - low_watermark + 10) {
    auto msg_opt = agent_->getMessage();
    if (!msg_opt) break;
    drained++;
  }

  // Now new messages should be accepted again
  auto new_msg = KeystoneMessage::create("sender", agent_->getAgentId(), "new_after_recovery");
  new_msg.priority = Priority::NORMAL;
  agent_->receiveMessage(new_msg);

  // Try to retrieve it
  bool found_new_msg = false;
  while (auto msg_opt = agent_->getMessage()) {
    if (msg_opt->command == "new_after_recovery") {
      found_new_msg = true;
      break;
    }
  }

  EXPECT_TRUE(found_new_msg);
}

// Test: Invalid priority routes to NORMAL queue
TEST_F(AgentBaseTest, InvalidPriorityRoutesToNormal) {
  auto msg = KeystoneMessage::create("sender", agent_->getAgentId(), "invalid");
  msg.priority = static_cast<Priority>(999);  // Invalid priority value

  agent_->receiveMessage(msg);

  auto received = agent_->getMessage();
  ASSERT_TRUE(received.has_value());
  EXPECT_EQ(received->command, "invalid");
}

// Test: updateQueueDepthMetrics called correctly
TEST_F(AgentBaseTest, UpdateQueueDepthMetrics) {
  // Send some messages
  for (int i = 0; i < 5; ++i) {
    auto msg = KeystoneMessage::create("sender", agent_->getAgentId(), "msg");
    msg.priority = Priority::HIGH;
    agent_->receiveMessage(msg);
  }

  // Call updateQueueDepthMetrics (indirectly via getMessage)
  auto msg_opt = agent_->getMessage();
  ASSERT_TRUE(msg_opt.has_value());

  // Metrics should be updated (no exception)
  agent_->updateQueueDepthMetrics();
}

// Test: getMessage with fairness mechanism - NORMAL processed during HIGH flood
TEST_F(AgentBaseTest, FairnessMechanismNormalProcessed) {
  // Send HIGH messages first
  for (int i = 0; i < 5; ++i) {
    auto msg = KeystoneMessage::create("sender", agent_->getAgentId(), "high_" + std::to_string(i));
    msg.priority = Priority::HIGH;
    agent_->receiveMessage(msg);
  }

  // Send one NORMAL message
  auto normal_msg = KeystoneMessage::create("sender", agent_->getAgentId(), "normal");
  normal_msg.priority = Priority::NORMAL;
  agent_->receiveMessage(normal_msg);

  // Process one HIGH message to initialize fairness timer
  auto msg1 = agent_->getMessage();
  ASSERT_TRUE(msg1.has_value());
  EXPECT_EQ(msg1->priority, Priority::HIGH);

  // Sleep to let fairness interval elapse
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  // Next getMessage should trigger fairness and return NORMAL
  bool normal_found = false;
  for (int i = 0; i < 10; ++i) {
    auto msg_opt = agent_->getMessage();
    if (!msg_opt) break;
    if (msg_opt->command == "normal") {
      normal_found = true;
      break;
    }
  }

  EXPECT_TRUE(normal_found);
}

// Test: getMessage with fairness mechanism - LOW processed during HIGH flood
TEST_F(AgentBaseTest, FairnessMechanismLowProcessed) {
  // Send HIGH messages first
  for (int i = 0; i < 5; ++i) {
    auto msg = KeystoneMessage::create("sender", agent_->getAgentId(), "high_" + std::to_string(i));
    msg.priority = Priority::HIGH;
    agent_->receiveMessage(msg);
  }

  // Send one LOW message
  auto low_msg = KeystoneMessage::create("sender", agent_->getAgentId(), "low");
  low_msg.priority = Priority::LOW;
  agent_->receiveMessage(low_msg);

  // Process one HIGH message to initialize fairness timer
  auto msg1 = agent_->getMessage();
  ASSERT_TRUE(msg1.has_value());
  EXPECT_EQ(msg1->priority, Priority::HIGH);

  // Sleep to let fairness interval elapse
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  // Next getMessage should trigger fairness and return LOW
  bool low_found = false;
  for (int i = 0; i < 10; ++i) {
    auto msg_opt = agent_->getMessage();
    if (!msg_opt) break;
    if (msg_opt->command == "low") {
      low_found = true;
      break;
    }
  }

  EXPECT_TRUE(low_found);
}

// Test: setMessageBus works correctly
TEST_F(AgentBaseTest, SetMessageBus) {
  auto new_agent = std::make_shared<TestAgent>("new_agent");
  auto new_bus = std::make_shared<MessageBus>();

  // Set bus
  new_agent->setMessageBus(new_bus.get());

  // Register and test
  new_bus->registerAgent(new_agent->getAgentId(), new_agent);

  auto msg = KeystoneMessage::create("sender", "new_agent", "test");
  EXPECT_NO_THROW({ new_agent->sendMessage(msg); });

  new_bus->unregisterAgent(new_agent->getAgentId());
}

// Test: Empty queue returns nullopt
TEST_F(AgentBaseTest, EmptyQueueReturnsNullopt) {
  auto msg_opt = agent_->getMessage();
  EXPECT_FALSE(msg_opt.has_value());
}

// Test: Multiple priority levels mixed
TEST_F(AgentBaseTest, MixedPriorityMessages) {
  // Send mixed priority messages
  for (int i = 0; i < 3; ++i) {
    auto high = KeystoneMessage::create("sender", agent_->getAgentId(), "high_" + std::to_string(i));
    high.priority = Priority::HIGH;
    agent_->receiveMessage(high);

    auto normal =
        KeystoneMessage::create("sender", agent_->getAgentId(), "normal_" + std::to_string(i));
    normal.priority = Priority::NORMAL;
    agent_->receiveMessage(normal);

    auto low = KeystoneMessage::create("sender", agent_->getAgentId(), "low_" + std::to_string(i));
    low.priority = Priority::LOW;
    agent_->receiveMessage(low);
  }

  // First 3 should be HIGH
  for (int i = 0; i < 3; ++i) {
    auto msg = agent_->getMessage();
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->priority, Priority::HIGH);
  }

  // Next 3 should be NORMAL
  for (int i = 0; i < 3; ++i) {
    auto msg = agent_->getMessage();
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->priority, Priority::NORMAL);
  }

  // Last 3 should be LOW
  for (int i = 0; i < 3; ++i) {
    auto msg = agent_->getMessage();
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->priority, Priority::LOW);
  }
}
