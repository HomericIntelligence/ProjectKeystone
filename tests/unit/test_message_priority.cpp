#include <gtest/gtest.h>
#include "agents/agent_base.hpp"
#include "core/message_bus.hpp"
#include "core/message.hpp"
#include <vector>
#include <string>

using namespace keystone;
using namespace keystone::core;
using namespace keystone::agents;

/**
 * @brief Test agent to verify priority-based message processing
 */
class TestPriorityAgent : public AgentBase {
public:
    explicit TestPriorityAgent(const std::string& agent_id)
        : AgentBase(agent_id) {}

    std::vector<std::string> processed_order;

    void processAllMessages() {
        while (auto msg = getMessage()) {
            // Record message content in order processed
            if (msg->payload) {
                processed_order.push_back(*msg->payload);
            }
        }
    }
};

/**
 * @brief Test that HIGH priority messages are processed first
 */
TEST(MessagePriorityTest, HighPriorityProcessedFirst) {
    auto agent = std::make_unique<TestPriorityAgent>("test_agent");

    // Send messages in NORMAL, HIGH, LOW order
    auto normal_msg = KeystoneMessage::create("sender", "test_agent", "cmd", "NORMAL");
    normal_msg.priority = Priority::NORMAL;

    auto high_msg = KeystoneMessage::create("sender", "test_agent", "cmd", "HIGH");
    high_msg.priority = Priority::HIGH;

    auto low_msg = KeystoneMessage::create("sender", "test_agent", "cmd", "LOW");
    low_msg.priority = Priority::LOW;

    // Receive in order: NORMAL, HIGH, LOW
    agent->receiveMessage(normal_msg);
    agent->receiveMessage(high_msg);
    agent->receiveMessage(low_msg);

    // Process all messages
    agent->processAllMessages();

    // Verify they were processed in priority order: HIGH, NORMAL, LOW
    ASSERT_EQ(agent->processed_order.size(), 3);
    EXPECT_EQ(agent->processed_order[0], "HIGH");
    EXPECT_EQ(agent->processed_order[1], "NORMAL");
    EXPECT_EQ(agent->processed_order[2], "LOW");
}

/**
 * @brief Test multiple messages of same priority maintain FIFO order
 */
TEST(MessagePriorityTest, SamePriorityFIFO) {
    auto agent = std::make_unique<TestPriorityAgent>("test_agent");

    // Send multiple NORMAL priority messages
    auto msg1 = KeystoneMessage::create("sender", "test_agent", "cmd", "FIRST");
    msg1.priority = Priority::NORMAL;

    auto msg2 = KeystoneMessage::create("sender", "test_agent", "cmd", "SECOND");
    msg2.priority = Priority::NORMAL;

    auto msg3 = KeystoneMessage::create("sender", "test_agent", "cmd", "THIRD");
    msg3.priority = Priority::NORMAL;

    agent->receiveMessage(msg1);
    agent->receiveMessage(msg2);
    agent->receiveMessage(msg3);

    agent->processAllMessages();

    // Same priority maintains FIFO order
    ASSERT_EQ(agent->processed_order.size(), 3);
    EXPECT_EQ(agent->processed_order[0], "FIRST");
    EXPECT_EQ(agent->processed_order[1], "SECOND");
    EXPECT_EQ(agent->processed_order[2], "THIRD");
}

/**
 * @brief Test complex mixed priority scenario
 */
TEST(MessagePriorityTest, MixedPriorityOrdering) {
    auto agent = std::make_unique<TestPriorityAgent>("test_agent");

    // Send messages in mixed order
    auto low1 = KeystoneMessage::create("sender", "test_agent", "cmd", "LOW1");
    low1.priority = Priority::LOW;

    auto normal1 = KeystoneMessage::create("sender", "test_agent", "cmd", "NORMAL1");
    normal1.priority = Priority::NORMAL;

    auto high1 = KeystoneMessage::create("sender", "test_agent", "cmd", "HIGH1");
    high1.priority = Priority::HIGH;

    auto normal2 = KeystoneMessage::create("sender", "test_agent", "cmd", "NORMAL2");
    normal2.priority = Priority::NORMAL;

    auto high2 = KeystoneMessage::create("sender", "test_agent", "cmd", "HIGH2");
    high2.priority = Priority::HIGH;

    auto low2 = KeystoneMessage::create("sender", "test_agent", "cmd", "LOW2");
    low2.priority = Priority::LOW;

    // Receive in mixed order
    agent->receiveMessage(low1);
    agent->receiveMessage(normal1);
    agent->receiveMessage(high1);
    agent->receiveMessage(normal2);
    agent->receiveMessage(high2);
    agent->receiveMessage(low2);

    agent->processAllMessages();

    // Expected order: HIGH1, HIGH2, NORMAL1, NORMAL2, LOW1, LOW2
    ASSERT_EQ(agent->processed_order.size(), 6);
    EXPECT_EQ(agent->processed_order[0], "HIGH1");
    EXPECT_EQ(agent->processed_order[1], "HIGH2");
    EXPECT_EQ(agent->processed_order[2], "NORMAL1");
    EXPECT_EQ(agent->processed_order[3], "NORMAL2");
    EXPECT_EQ(agent->processed_order[4], "LOW1");
    EXPECT_EQ(agent->processed_order[5], "LOW2");
}

/**
 * @brief Test that default priority is NORMAL
 */
TEST(MessagePriorityTest, DefaultPriorityIsNormal) {
    auto msg = KeystoneMessage::create("sender", "receiver", "cmd", "data");

    // Default priority should be NORMAL
    EXPECT_EQ(msg.priority, Priority::NORMAL);
}

/**
 * @brief Test priority with getMessage() one at a time
 */
TEST(MessagePriorityTest, GetMessageRespectsPriority) {
    auto agent = std::make_unique<TestPriorityAgent>("test_agent");

    // Send LOW, NORMAL, HIGH
    auto low_msg = KeystoneMessage::create("sender", "test_agent", "cmd", "LOW");
    low_msg.priority = Priority::LOW;

    auto normal_msg = KeystoneMessage::create("sender", "test_agent", "cmd", "NORMAL");
    normal_msg.priority = Priority::NORMAL;

    auto high_msg = KeystoneMessage::create("sender", "test_agent", "cmd", "HIGH");
    high_msg.priority = Priority::HIGH;

    agent->receiveMessage(low_msg);
    agent->receiveMessage(normal_msg);
    agent->receiveMessage(high_msg);

    // Get messages one at a time
    auto msg1 = agent->getMessage();
    ASSERT_TRUE(msg1.has_value());
    EXPECT_EQ(*msg1->payload, "HIGH");

    auto msg2 = agent->getMessage();
    ASSERT_TRUE(msg2.has_value());
    EXPECT_EQ(*msg2->payload, "NORMAL");

    auto msg3 = agent->getMessage();
    ASSERT_TRUE(msg3.has_value());
    EXPECT_EQ(*msg3->payload, "LOW");

    // No more messages
    auto msg4 = agent->getMessage();
    EXPECT_FALSE(msg4.has_value());
}
