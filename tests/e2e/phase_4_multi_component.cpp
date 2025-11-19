#include <gtest/gtest.h>
#include "agents/async_chief_architect_agent.hpp"
#include "agents/async_component_lead_agent.hpp"
#include "core/message_bus.hpp"
#include "concurrency/work_stealing_scheduler.hpp"
#include <memory>

using namespace keystone::agents;
using namespace keystone::core;
using namespace keystone::concurrency;

/**
 * Phase 4 E2E Tests: Multi-Component Hierarchy
 *
 * Tests the full 4-layer HMAS with multiple components executing concurrently.
 */
class Phase4MultiComponentTest : public ::testing::Test {
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
 * Test: ChiefArchitect registers 4 components
 */
TEST_F(Phase4MultiComponentTest, RegisterFourComponents) {
    auto chief = std::make_unique<AsyncChiefArchitectAgent>("chief");
    chief->setMessageBus(message_bus_.get());
    chief->setScheduler(scheduler_.get());

    // Create 4 component leads
    auto core_lead = std::make_unique<AsyncComponentLeadAgent>("core_lead");
    auto protocol_lead = std::make_unique<AsyncComponentLeadAgent>("protocol_lead");
    auto agents_lead = std::make_unique<AsyncComponentLeadAgent>("agents_lead");
    auto integration_lead = std::make_unique<AsyncComponentLeadAgent>("integration_lead");

    core_lead->setMessageBus(message_bus_.get());
    protocol_lead->setMessageBus(message_bus_.get());
    agents_lead->setMessageBus(message_bus_.get());
    integration_lead->setMessageBus(message_bus_.get());

    // Register all components with chief
    chief->registerComponent("Core", core_lead.get());
    chief->registerComponent("Protocol", protocol_lead.get());
    chief->registerComponent("Agents", agents_lead.get());
    chief->registerComponent("Integration", integration_lead.get());

    // Verify registration
    EXPECT_EQ(chief->getComponentCount(), 4);

    auto names = chief->getComponentNames();
    EXPECT_EQ(names.size(), 4);

    // Check all component names present
    EXPECT_NE(std::find(names.begin(), names.end(), "Core"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "Protocol"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "Agents"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "Integration"), names.end());
}

/**
 * Test: Component dependencies are resolved (topological sort)
 */
TEST_F(Phase4MultiComponentTest, ComponentDependencyResolution) {
    auto chief = std::make_unique<AsyncChiefArchitectAgent>("chief");

    // Create components
    auto core_lead = std::make_unique<AsyncComponentLeadAgent>("core_lead");
    auto protocol_lead = std::make_unique<AsyncComponentLeadAgent>("protocol_lead");
    auto agents_lead = std::make_unique<AsyncComponentLeadAgent>("agents_lead");
    auto integration_lead = std::make_unique<AsyncComponentLeadAgent>("integration_lead");

    chief->registerComponent("Core", core_lead.get());
    chief->registerComponent("Protocol", protocol_lead.get());
    chief->registerComponent("Agents", agents_lead.get());
    chief->registerComponent("Integration", integration_lead.get());

    // Add dependencies:
    // - Agents depends on Core and Protocol
    // - Integration depends on Agents
    chief->addComponentDependency("Agents", {"Core", "Protocol"});
    chief->addComponentDependency("Integration", {"Agents"});

    // Get execution order
    auto execution_order = chief->getComponentExecutionOrder();

    // Verify order respects dependencies
    ASSERT_EQ(execution_order.size(), 4);

    // Find indices
    auto core_idx = std::find(execution_order.begin(), execution_order.end(), "Core") - execution_order.begin();
    auto protocol_idx = std::find(execution_order.begin(), execution_order.end(), "Protocol") - execution_order.begin();
    auto agents_idx = std::find(execution_order.begin(), execution_order.end(), "Agents") - execution_order.begin();
    auto integration_idx = std::find(execution_order.begin(), execution_order.end(), "Integration") - execution_order.begin();

    // Verify: Core and Protocol come before Agents
    EXPECT_LT(core_idx, agents_idx);
    EXPECT_LT(protocol_idx, agents_idx);

    // Verify: Agents comes before Integration
    EXPECT_LT(agents_idx, integration_idx);
}

/**
 * Test: Circular dependency detection
 */
TEST_F(Phase4MultiComponentTest, CircularDependencyDetection) {
    auto chief = std::make_unique<AsyncChiefArchitectAgent>("chief");

    auto comp_a = std::make_unique<AsyncComponentLeadAgent>("comp_a");
    auto comp_b = std::make_unique<AsyncComponentLeadAgent>("comp_b");
    auto comp_c = std::make_unique<AsyncComponentLeadAgent>("comp_c");

    chief->registerComponent("A", comp_a.get());
    chief->registerComponent("B", comp_b.get());
    chief->registerComponent("C", comp_c.get());

    // Create circular dependency: A → B → C → A
    chief->addComponentDependency("A", {"B"});
    chief->addComponentDependency("B", {"C"});
    chief->addComponentDependency("C", {"A"});  // Creates cycle

    // Should throw on circular dependency
    EXPECT_THROW({
        auto execution_order = chief->getComponentExecutionOrder();
    }, std::runtime_error);
}

/**
 * Test: Independent components (no dependencies)
 */
TEST_F(Phase4MultiComponentTest, IndependentComponentsExecutionOrder) {
    auto chief = std::make_unique<AsyncChiefArchitectAgent>("chief");

    auto core_lead = std::make_unique<AsyncComponentLeadAgent>("core_lead");
    auto protocol_lead = std::make_unique<AsyncComponentLeadAgent>("protocol_lead");

    chief->registerComponent("Core", core_lead.get());
    chief->registerComponent("Protocol", protocol_lead.get());

    // No dependencies - both independent
    auto execution_order = chief->getComponentExecutionOrder();

    // Both should be in the list
    ASSERT_EQ(execution_order.size(), 2);
    EXPECT_TRUE(std::find(execution_order.begin(), execution_order.end(), "Core") != execution_order.end());
    EXPECT_TRUE(std::find(execution_order.begin(), execution_order.end(), "Protocol") != execution_order.end());
}
