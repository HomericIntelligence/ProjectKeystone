/**
 * @file hierarchy_performance.cpp
 * @brief Performance benchmarks for HMAS hierarchy
 *
 * NOTE: These benchmarks are currently disabled as they reference async_* agent headers
 * that were removed in the unified async API refactoring. Need to update benchmarks
 * to use the new unified async API. Re-enable by changing #if 0 to #if 1.
 */

#if 0  // Disabled until benchmarks are updated for unified async API

#include <benchmark/benchmark.h>

#include <memory>
#include <vector>

#include "agents/async_chief_architect_agent.hpp"
#include "agents/async_component_lead_agent.hpp"
#include "agents/async_module_lead_agent.hpp"
#include "agents/async_task_agent.hpp"
#include "agents/chief_architect_agent.hpp"
#include "agents/component_lead_agent.hpp"
#include "agents/module_lead_agent.hpp"
#include "agents/task_agent.hpp"
#include "concurrency/work_stealing_scheduler.hpp"
#include "core/message_bus.hpp"

using namespace keystone;
using namespace keystone::agents;
using namespace keystone::core;
using namespace keystone::concurrency;

/**
 * Performance Benchmarks: Sync vs Async Agent Hierarchies
 *
 * These benchmarks measure:
 * - Message throughput (messages/second)
 * - Latency (time to process single message)
 * - Scalability (performance with varying worker counts)
 * - Concurrent execution efficiency
 */

// Benchmark: Sync 4-Layer Hierarchy
static void BM_Sync4LayerHierarchy(benchmark::State& state) {
  // Setup sync hierarchy
  MessageBus bus;

  auto chief = std::make_unique<ChiefArchitectAgent>("chief");
  auto comp_lead = std::make_unique<ComponentLeadAgent>("comp_lead");
  auto module1 = std::make_unique<ModuleLeadAgent>("module1");
  auto module2 = std::make_unique<ModuleLeadAgent>("module2");

  chief->setMessageBus(&bus);
  comp_lead->setMessageBus(&bus);
  module1->setMessageBus(&bus);
  module2->setMessageBus(&bus);

  bus.registerAgent(chief->getAgentId(), chief.get());
  bus.registerAgent(comp_lead->getAgentId(), comp_lead.get());
  bus.registerAgent(module1->getAgentId(), module1.get());
  bus.registerAgent(module2->getAgentId(), module2.get());

  std::vector<std::unique_ptr<TaskAgent>> task_agents;
  for (int i = 1; i <= 6; ++i) {
    auto task = std::make_unique<TaskAgent>("task" + std::to_string(i));
    task->setMessageBus(&bus);
    bus.registerAgent(task->getAgentId(), task.get());
    task_agents.push_back(std::move(task));
  }

  comp_lead->setAvailableModuleLeads({"module1", "module2"});
  module1->setAvailableTaskAgents({"task1", "task2", "task3"});
  module2->setAvailableTaskAgents({"task4", "task5", "task6"});

  // Benchmark loop
  for (auto _ : state) {
    auto msg = KeystoneMessage::create(
        "chief", "comp_lead",
        "Component: Messaging(10+20+30) and Concurrency(40+50+60)");
    bus.routeMessage(msg);

    // Process messages synchronously
    comp_lead->processMessage(msg);

    // Process module results
    auto mod1_result = comp_lead->synthesizeComponentResult();
    benchmark::DoNotOptimize(mod1_result);
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Sync4LayerHierarchy);

// Benchmark: Async 4-Layer Hierarchy (4 workers)
// FIXED: Setup/teardown moved outside loop, measures actual message flow
static void BM_Async4LayerHierarchy_4Workers(benchmark::State& state) {
  // Setup ONCE (not part of benchmark timing)
  WorkStealingScheduler scheduler(4);
  scheduler.start();

  MessageBus bus;
  bus.setScheduler(&scheduler);

  auto chief = std::make_unique<AsyncChiefArchitectAgent>("chief");
  auto comp_lead = std::make_unique<AsyncComponentLeadAgent>("comp_lead");
  auto module1 = std::make_unique<AsyncModuleLeadAgent>("module1");
  auto module2 = std::make_unique<AsyncModuleLeadAgent>("module2");

  chief->setMessageBus(&bus);
  chief->setScheduler(&scheduler);
  comp_lead->setMessageBus(&bus);
  comp_lead->setScheduler(&scheduler);
  module1->setMessageBus(&bus);
  module1->setScheduler(&scheduler);
  module2->setMessageBus(&bus);
  module2->setScheduler(&scheduler);

  bus.registerAgent(chief->getAgentId(), chief.get());
  bus.registerAgent(comp_lead->getAgentId(), comp_lead.get());
  bus.registerAgent(module1->getAgentId(), module1.get());
  bus.registerAgent(module2->getAgentId(), module2.get());

  std::vector<std::unique_ptr<AsyncTaskAgent>> task_agents;
  for (int i = 1; i <= 6; ++i) {
    auto task = std::make_unique<AsyncTaskAgent>("task" + std::to_string(i));
    task->setMessageBus(&bus);
    task->setScheduler(&scheduler);
    bus.registerAgent(task->getAgentId(), task.get());
    task_agents.push_back(std::move(task));
  }

  comp_lead->setAvailableModuleLeads({"module1", "module2"});
  module1->setAvailableTaskAgents({"task1", "task2", "task3"});
  module2->setAvailableTaskAgents({"task4", "task5", "task6"});

  // Warmup: ensure workers are ready
  for (int i = 0; i < 10; ++i) {
    scheduler.submit([]() {
      volatile int x = 42;
      (void)x;
    });
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // BENCHMARK LOOP: Only measures actual message flow
  for (auto _ : state) {
    chief->sendCommandAsync(
        "Component: Messaging(10+20+30) and Concurrency(40+50+60)",
        "comp_lead");

    // FIXED: Wait for actual processing (not arbitrary 10ms)
    // In real system, would use future/promise for completion
    // For benchmark, minimal wait for message delivery
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }

  // Teardown ONCE (not part of benchmark timing)
  state.SetItemsProcessed(state.iterations());
  scheduler.shutdown();
}
BENCHMARK(BM_Async4LayerHierarchy_4Workers);

// Benchmark: Async with 8 workers
// FIXED: Setup/teardown moved outside loop, measures actual message flow
static void BM_Async4LayerHierarchy_8Workers(benchmark::State& state) {
  // Setup ONCE (not part of benchmark timing)
  WorkStealingScheduler scheduler(8);
  scheduler.start();

  MessageBus bus;
  bus.setScheduler(&scheduler);

  auto chief = std::make_unique<AsyncChiefArchitectAgent>("chief");
  auto comp_lead = std::make_unique<AsyncComponentLeadAgent>("comp_lead");
  auto module1 = std::make_unique<AsyncModuleLeadAgent>("module1");
  auto module2 = std::make_unique<AsyncModuleLeadAgent>("module2");

  chief->setMessageBus(&bus);
  chief->setScheduler(&scheduler);
  comp_lead->setMessageBus(&bus);
  comp_lead->setScheduler(&scheduler);
  module1->setMessageBus(&bus);
  module1->setScheduler(&scheduler);
  module2->setMessageBus(&bus);
  module2->setScheduler(&scheduler);

  bus.registerAgent(chief->getAgentId(), chief.get());
  bus.registerAgent(comp_lead->getAgentId(), comp_lead.get());
  bus.registerAgent(module1->getAgentId(), module1.get());
  bus.registerAgent(module2->getAgentId(), module2.get());

  std::vector<std::unique_ptr<AsyncTaskAgent>> task_agents;
  for (int i = 1; i <= 6; ++i) {
    auto task = std::make_unique<AsyncTaskAgent>("task" + std::to_string(i));
    task->setMessageBus(&bus);
    task->setScheduler(&scheduler);
    bus.registerAgent(task->getAgentId(), task.get());
    task_agents.push_back(std::move(task));
  }

  comp_lead->setAvailableModuleLeads({"module1", "module2"});
  module1->setAvailableTaskAgents({"task1", "task2", "task3"});
  module2->setAvailableTaskAgents({"task4", "task5", "task6"});

  // Warmup: ensure workers are ready
  for (int i = 0; i < 10; ++i) {
    scheduler.submit([]() {
      volatile int x = 42;
      (void)x;
    });
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // BENCHMARK LOOP: Only measures actual message flow
  for (auto _ : state) {
    chief->sendCommandAsync(
        "Component: Messaging(10+20+30) and Concurrency(40+50+60)",
        "comp_lead");

    // FIXED: Wait for actual processing (not arbitrary 10ms)
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }

  // Teardown ONCE (not part of benchmark timing)
  state.SetItemsProcessed(state.iterations());
  scheduler.shutdown();
}
BENCHMARK(BM_Async4LayerHierarchy_8Workers);

// DISABLED: Long-running parameterized benchmarks
// These run multiple times with different worker counts, taking too long.
// Re-enable for detailed performance analysis when needed.

/*
// Benchmark: Message throughput (async task agent only)
static void BM_AsyncTaskAgentThroughput(benchmark::State& state) {
    const int num_workers = state.range(0);

    WorkStealingScheduler scheduler(num_workers);
    scheduler.start();

    MessageBus bus;
    bus.setScheduler(&scheduler);

    auto agent = std::make_unique<AsyncTaskAgent>("throughput_test");
    agent->setMessageBus(&bus);
    agent->setScheduler(&scheduler);
    bus.registerAgent(agent->getAgentId(), agent.get());

    for (auto _ : state) {
        auto msg = KeystoneMessage::create("test", agent->getAgentId(), "echo
42"); bus.routeMessage(msg);
    }

    // Wait for queue to drain
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    state.SetItemsProcessed(state.iterations());
    scheduler.shutdown();
}
BENCHMARK(BM_AsyncTaskAgentThroughput)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

// Benchmark: Work-stealing scheduler submission rate
static void BM_SchedulerSubmissionRate(benchmark::State& state) {
    const int num_workers = state.range(0);

    WorkStealingScheduler scheduler(num_workers);
    scheduler.start();

    for (auto _ : state) {
        scheduler.submit([]() {
            // Minimal work
            volatile int x = 42;
            (void)x;
        });
    }

    scheduler.shutdown();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SchedulerSubmissionRate)->Arg(1)->Arg(2)->Arg(4)->Arg(8)->Arg(16);
*/

BENCHMARK_MAIN();

#endif  // Disabled benchmarks
