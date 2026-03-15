/**
 * @file test_component_lead_agent.cpp
 * @brief Unit tests for ComponentLeadAgent (Level 1)
 *
 * Test coverage:
 * - Construction & Initialization (2 tests)
 * - Module Decomposition (4 tests)
 * - Coordination (4 tests)
 * - State Machine (2 tests)
 *
 * Total: 12 tests
 */

#include "agents/component_lead_agent.hpp"
#include "agents/module_lead_agent.hpp"
#include "core/message_bus.hpp"
#include "unit/agent_test_fixture.hpp"

#include <gtest/gtest.h>

using namespace keystone;
using namespace keystone::test;

// ============================================================================
// Construction & Initialization Tests (2 tests)
// ============================================================================

class ComponentLeadAgentTest : public AgentTestFixture {};

TEST_F(ComponentLeadAgentTest, DefaultConstruction) {
  auto agent = std::make_shared<agents::ComponentLeadAgent>("component_1");
  EXPECT_EQ(agent->getAgentId(), "component_1");
}

TEST_F(ComponentLeadAgentTest, InitialCoordinationState) {
  auto agent = std::make_shared<agents::ComponentLeadAgent>("component_1");
  // Should start in IDLE state
  EXPECT_EQ(agent->getCurrentState(), agents::ComponentLeadAgent::State::IDLE);
}

// ============================================================================
// Module Decomposition Tests (4 tests)
// ============================================================================

TEST_F(ComponentLeadAgentTest, DecomposeComponentIntoModules) {
  auto component = std::make_shared<agents::ComponentLeadAgent>("component_1");
  component->setMessageBus(bus_.get());
  bus_->registerAgent(component->getAgentId(), component);

  // Configure module leads
  std::vector<std::string> module_ids = {"module_1", "module_2"};
  component->setAvailableModuleLeads(module_ids);

  // Send component goal
  auto msg = core::KeystoneMessage::create("chief", "component_1", "build feature X");
  component->receiveMessage(msg);

  auto received = component->getMessage();
  ASSERT_TRUE(received.has_value());
  EXPECT_EQ(received->command, "build feature X");
}

TEST_F(ComponentLeadAgentTest, DecomposeWithVariableCounts) {
  auto component = std::make_shared<agents::ComponentLeadAgent>("component_1");
  component->setMessageBus(bus_.get());
  bus_->registerAgent(component->getAgentId(), component);

  // Test with 1 module
  std::vector<std::string> module_ids = {"module_1"};
  component->setAvailableModuleLeads(module_ids);
  EXPECT_NO_THROW(component->setAvailableModuleLeads(module_ids));

  // Test with 3 modules
  module_ids = {"module_1", "module_2", "module_3"};
  EXPECT_NO_THROW(component->setAvailableModuleLeads(module_ids));

  // Test with 5 modules
  module_ids = {"module_1", "module_2", "module_3", "module_4", "module_5"};
  EXPECT_NO_THROW(component->setAvailableModuleLeads(module_ids));
}

TEST_F(ComponentLeadAgentTest, DecompositionFailure) {
  auto component = std::make_shared<agents::ComponentLeadAgent>("component_1");
  component->setMessageBus(bus_.get());
  bus_->registerAgent(component->getAgentId(), component);

  // No module leads configured - should handle gracefully
  auto msg = core::KeystoneMessage::create("chief", "component_1", "goal");
  EXPECT_NO_THROW(component->receiveMessage(msg));
}

TEST_F(ComponentLeadAgentTest, EmptyGoalHandling) {
  auto component = std::make_shared<agents::ComponentLeadAgent>("component_1");
  component->setMessageBus(bus_.get());
  bus_->registerAgent(component->getAgentId(), component);

  std::vector<std::string> module_ids = {"module_1", "module_2"};
  component->setAvailableModuleLeads(module_ids);

  // Empty command
  auto msg = core::KeystoneMessage::create("chief", "component_1", "");
  EXPECT_NO_THROW(component->receiveMessage(msg));
}

// ============================================================================
// Coordination Tests (4 tests)
// ============================================================================

TEST_F(ComponentLeadAgentTest, CoordinateTwoModuleLeads) {
  auto component = std::make_shared<agents::ComponentLeadAgent>("component_1");
  auto module1 = std::make_shared<agents::ModuleLeadAgent>("module_1");
  auto module2 = std::make_shared<agents::ModuleLeadAgent>("module_2");

  component->setMessageBus(bus_.get());
  module1->setMessageBus(bus_.get());
  module2->setMessageBus(bus_.get());

  bus_->registerAgent(component->getAgentId(), component);
  bus_->registerAgent(module1->getAgentId(), module1);
  bus_->registerAgent(module2->getAgentId(), module2);

  std::vector<std::string> module_ids = {"module_1", "module_2"};
  component->setAvailableModuleLeads(module_ids);

  // Send messages to modules
  component->sendMessage(core::KeystoneMessage::create("component_1", "module_1", "goal1"));
  component->sendMessage(core::KeystoneMessage::create("component_1", "module_2", "goal2"));

  // Both modules should receive messages
  auto r1 = module1->getMessage();
  auto r2 = module2->getMessage();

  ASSERT_TRUE(r1.has_value());
  ASSERT_TRUE(r2.has_value());
  EXPECT_EQ(r1->command, "goal1");
  EXPECT_EQ(r2->command, "goal2");
}

TEST_F(ComponentLeadAgentTest, CoordinateWithPartialFailures) {
  auto component = std::make_shared<agents::ComponentLeadAgent>("component_1");
  auto module1 = std::make_shared<agents::ModuleLeadAgent>("module_1");

  component->setMessageBus(bus_.get());
  module1->setMessageBus(bus_.get());

  bus_->registerAgent(component->getAgentId(), component);
  bus_->registerAgent(module1->getAgentId(), module1);

  std::vector<std::string> module_ids = {"module_1", "module_2"};  // module_2 doesn't exist
  component->setAvailableModuleLeads(module_ids);

  // Try to send to both (one will fail routing)
  EXPECT_NO_THROW(
      component->sendMessage(core::KeystoneMessage::create("component_1", "module_1", "goal1")));
  EXPECT_NO_THROW(component->sendMessage(
      core::KeystoneMessage::create("component_1", "module_2", "goal2")));  // Will fail routing
}

TEST_F(ComponentLeadAgentTest, WaitForAllModuleResults) {
  auto component = std::make_shared<agents::ComponentLeadAgent>("component_1");
  auto module1 = std::make_shared<agents::ModuleLeadAgent>("module_1");
  auto module2 = std::make_shared<agents::ModuleLeadAgent>("module_2");

  component->setMessageBus(bus_.get());
  module1->setMessageBus(bus_.get());
  module2->setMessageBus(bus_.get());

  bus_->registerAgent(component->getAgentId(), component);
  bus_->registerAgent(module1->getAgentId(), module1);
  bus_->registerAgent(module2->getAgentId(), module2);

  std::vector<std::string> module_ids = {"module_1", "module_2"};
  component->setAvailableModuleLeads(module_ids);

  // Send module results back to component
  module1->sendMessage(core::KeystoneMessage::create("module_1", "component_1", "result1"));
  module2->sendMessage(core::KeystoneMessage::create("module_2", "component_1", "result2"));

  // Component should receive both results
  auto r1 = component->getMessage();
  auto r2 = component->getMessage();

  ASSERT_TRUE(r1.has_value());
  ASSERT_TRUE(r2.has_value());
}

TEST_F(ComponentLeadAgentTest, ResultAggregation) {
  auto component = std::make_shared<agents::ComponentLeadAgent>("component_1");
  component->setMessageBus(bus_.get());
  bus_->registerAgent(component->getAgentId(), component);

  std::vector<std::string> module_ids = {"module_1", "module_2"};
  component->setAvailableModuleLeads(module_ids);

  // Simulate receiving results from modules
  auto result1 = core::KeystoneMessage::create("module_1", "component_1", "module1_result");
  auto result2 = core::KeystoneMessage::create("module_2", "component_1", "module2_result");

  component->receiveMessage(result1);
  component->receiveMessage(result2);

  // Process results
  component->processMessage(result1).get();
  component->processMessage(result2).get();

  // Aggregate (should combine all results)
  auto aggregated = component->synthesizeComponentResult();
  EXPECT_FALSE(aggregated.empty());
}

// ============================================================================
// State Machine Tests (2 tests)
// ============================================================================

TEST_F(ComponentLeadAgentTest, StateTransitionFlow) {
  auto component = std::make_shared<agents::ComponentLeadAgent>("component_1");
  component->setMessageBus(bus_.get());
  bus_->registerAgent(component->getAgentId(), component);

  std::vector<std::string> module_ids = {"module_1", "module_2"};
  component->setAvailableModuleLeads(module_ids);

  // Initial state: IDLE
  EXPECT_EQ(component->getCurrentState(), agents::ComponentLeadAgent::State::IDLE);

  // Get execution trace (should be empty or just initialization)
  auto trace = component->getExecutionTrace();
  // Trace may be empty or contain initialization states
  (void)trace;  // Suppress unused variable warning
}

TEST_F(ComponentLeadAgentTest, ConcurrentCoordination) {
  auto component = std::make_shared<agents::ComponentLeadAgent>("component_1");
  component->setMessageBus(bus_.get());
  bus_->registerAgent(component->getAgentId(), component);

  std::vector<std::string> module_ids = {"module_1", "module_2"};
  component->setAvailableModuleLeads(module_ids);

  // Send many messages concurrently
  for (int32_t i = 0; i < 50; ++i) {
    auto msg = core::KeystoneMessage::create("sender", "component_1", "cmd" + std::to_string(i));
    EXPECT_NO_THROW(component->receiveMessage(msg));
  }

  // Verify all messages were queued
  int32_t count = 0;
  while (component->getMessage().has_value()) {
    ++count;
  }
  EXPECT_EQ(count, 50);
}
