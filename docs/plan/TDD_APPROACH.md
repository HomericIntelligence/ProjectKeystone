# ProjectKeystone TDD Implementation Plan

## Overview

This document outlines a **Test-Driven Development (TDD)** approach for ProjectKeystone, with **End-to-End (E2E) execution as the primary testing method**. We'll build incrementally, starting with a minimal two-agent system and expanding to the full three-layer hierarchy only after validating each component works in real execution scenarios.

## TDD Philosophy for Keystone

### Core Principles

1. **E2E Tests First**: Write complete workflow tests before implementing components
2. **Red-Green-Refactor**: Write failing E2E test → Make it pass → Refactor
3. **Incremental Complexity**: Start simple (2 agents) → Add features → Scale to 3 layers
4. **Always Executable**: Every commit should have a working E2E test that demonstrates progress
5. **Real-World Scenarios**: Tests mirror actual production workflows

### Why E2E Testing First?

**Traditional Approach Problems:**
- Unit tests can pass while integration fails
- Mock-heavy tests don't catch real issues
- Performance problems only discovered late
- Architecture validated only after full implementation

**E2E Testing Benefits:**
- Validates entire message flow immediately
- Catches integration issues early
- Performance testing from day one
- Architecture validated continuously
- Confidence in each increment

## Simplified Two-Agent Model (Phase 1)

### Architecture: Coordinator + Worker

```
┌─────────────────────┐
│   Coordinator       │  - Receives user goals
│   (Single Agent)    │  - Decomposes into tasks
│                     │  - Aggregates results
└──────────┬──────────┘
           │
           │ Delegates tasks via messages
           │
           ▼
┌─────────────────────┐
│   Worker            │  - Receives specific tasks
│   (Single Agent)    │  - Executes work
│                     │  - Returns results
└─────────────────────┘
```

**Key Simplifications:**
- Only 2 agent types (no Branch layer yet)
- Single Coordinator, single Worker initially
- Focus on message passing and coroutine infrastructure
- Defer hierarchical complexity until proven

### E2E Test Scenario 1: Simple Task Delegation

```cpp
// tests/e2e/SimpleDelegationTest.cpp

TEST(E2E_SimpleDelegation, CoordinatorDelegatesTaskToWorker) {
    // ARRANGE: Set up the two-agent system
    ThreadPool pool{4};
    auto coordinator = std::make_unique<CoordinatorAgent>();
    auto worker = std::make_unique<WorkerAgent>();

    coordinator->initialize();
    worker->initialize();

    // Start agent event loops
    pool.submit([&]() { coordinator->run(); });
    pool.submit([&]() { worker->run(); });

    // ACT: Send a goal to the coordinator
    std::string goal = "Calculate: 2 + 2";
    auto result_future = coordinator->processGoal(goal);

    // ASSERT: Verify end-to-end execution
    auto result = result_future.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(result, std::future_status::ready);

    std::string response = result_future.get();
    EXPECT_EQ(response, "Result: 4");

    // Verify message flow
    EXPECT_EQ(coordinator->getMessagesSent(), 1);  // Task to worker
    EXPECT_EQ(worker->getMessagesReceived(), 1);   // Task received
    EXPECT_EQ(worker->getMessagesSent(), 1);       // Result sent
    EXPECT_EQ(coordinator->getMessagesReceived(), 1); // Result received

    // Clean shutdown
    coordinator->shutdown();
    worker->shutdown();
    pool.shutdown();
}
```

## Incremental TDD Phases

### Phase 0: E2E Test Infrastructure (Week 1)

**Goal**: Set up the ability to write and run E2E tests

**TDD Cycle 1: Minimal E2E Test Harness**

1. **RED - Write the test** (even though nothing exists):
```cpp
TEST(E2E_Foundation, CanCreateAndRunTwoAgents) {
    // This will fail - nothing exists yet
    ThreadPool pool{2};
    auto agent1 = std::make_unique<MinimalAgent>("agent1");
    auto agent2 = std::make_unique<MinimalAgent>("agent2");

    agent1->initialize();
    agent2->initialize();

    EXPECT_TRUE(agent1->isRunning());
    EXPECT_TRUE(agent2->isRunning());
}
```

2. **GREEN - Minimal implementation**:
   - Create `MinimalAgent` stub class
   - Create `ThreadPool` stub
   - Make test compile and pass (even with no-op implementations)

3. **REFACTOR**:
   - Clean up code structure
   - Add proper includes and modules
   - Document patterns

**Deliverables Week 1:**
- ✅ E2E test framework set up (GoogleTest)
- ✅ CMake configured to build and run E2E tests
- ✅ Minimal agent stub that can be instantiated
- ✅ Thread pool stub that can submit work
- ✅ First E2E test passes (even if trivial)

---

### Phase 1: Core Message Passing (Weeks 2-3)

**Goal**: Two agents can send messages to each other

**E2E Test Scenario:**
```cpp
TEST(E2E_MessagePassing, AgentCanSendMessageToAnotherAgent) {
    // RED: Write test first
    ThreadPool pool{2};
    MessageBus bus;

    auto sender = std::make_unique<TestAgent>("sender");
    auto receiver = std::make_unique<TestAgent>("receiver");

    bus.registerAgent(sender.get());
    bus.registerAgent(receiver.get());

    // Create message
    KeystoneMessage msg = MessageBuilder()
        .from("sender")
        .to("receiver")
        .action(ActionType::TEST)
        .payload("Hello")
        .build();

    // Send message
    sender->send(msg);

    // Verify receiver got it
    auto received = receiver->waitForMessage(std::chrono::seconds(1));
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->payload_as_string(), "Hello");
}
```

**TDD Workflow:**

**Iteration 1: Message Structure**
1. RED: Test fails (KeystoneMessage doesn't exist)
2. GREEN: Create minimal `KeystoneMessage` struct
3. REFACTOR: Add proper fields, serialization

**Iteration 2: Message Queue**
1. RED: Test fails (agents can't send/receive)
2. GREEN: Add `concurrentqueue` mailbox to agents
3. REFACTOR: Wrap in `MessageQueue` class

**Iteration 3: Message Bus**
1. RED: Test fails (no routing between agents)
2. GREEN: Create `MessageBus` to route messages
3. REFACTOR: Make routing efficient, add error handling

**Iteration 4: Coroutine Integration**
1. RED: Blocking waits are inefficient
2. GREEN: Add `co_await` for message receiving
3. REFACTOR: Create custom `AsyncQueuePop` awaitable

**Deliverables Weeks 2-3:**
- ✅ `KeystoneMessage` structure with serialization
- ✅ `MessageQueue` with `concurrentqueue`
- ✅ `MessageBus` for agent-to-agent routing
- ✅ Custom awaitable for async message receiving
- ✅ E2E test: Agent A sends message to Agent B successfully
- ✅ E2E test: Multiple messages in sequence
- ✅ E2E test: Bidirectional communication

---

### Phase 2: Coordinator-Worker Pattern (Weeks 4-5)

**Goal**: Coordinator delegates task to Worker, receives result

**E2E Test Scenario:**
```cpp
TEST(E2E_Delegation, CoordinatorDelegatesAndAggregates) {
    // Setup
    ThreadPool pool{4};
    MessageBus bus;

    auto coordinator = std::make_unique<CoordinatorAgent>();
    auto worker = std::make_unique<WorkerAgent>();

    bus.registerAgent(coordinator.get());
    bus.registerAgent(worker.get());

    // Start agent loops
    pool.submit([&]() { coordinator->run(); });
    pool.submit([&]() { worker->run(); });

    // RED: Define the workflow we want
    std::string goal = "Process: [1, 2, 3, 4, 5]";
    auto result_future = coordinator->processGoal(goal);

    // Expected: Coordinator sends to Worker, Worker processes, returns result
    auto result = result_future.get();
    EXPECT_EQ(result, "Sum: 15");

    // Verify execution trace
    auto trace = coordinator->getExecutionTrace();
    ASSERT_EQ(trace.size(), 3);
    EXPECT_EQ(trace[0], "Received goal");
    EXPECT_EQ(trace[1], "Delegated to worker");
    EXPECT_EQ(trace[2], "Received result from worker");
}
```

**TDD Workflow:**

**Iteration 1: Agent State Machine**
1. RED: Agents need lifecycle states
2. GREEN: Add `AgentState` enum and transitions
3. REFACTOR: Implement proper FSM with validation

**Iteration 2: Coordinator Logic**
1. RED: Coordinator doesn't decompose tasks
2. GREEN: Add task decomposition to Coordinator
3. REFACTOR: Make decomposition configurable

**Iteration 3: Worker Execution**
1. RED: Worker doesn't process tasks
2. GREEN: Add task execution to Worker
3. REFACTOR: Create tool execution framework

**Iteration 4: Result Aggregation**
1. RED: Coordinator doesn't aggregate results
2. GREEN: Add result handling to Coordinator
3. REFACTOR: Support multiple result formats

**Deliverables Weeks 4-5:**
- ✅ `CoordinatorAgent` with task decomposition
- ✅ `WorkerAgent` with task execution
- ✅ Agent state machine (IDLE → PLANNING → EXECUTING → AGGREGATING)
- ✅ E2E test: Simple task delegation and result return
- ✅ E2E test: Multiple workers (1 coordinator, 3 workers)
- ✅ E2E test: Error handling when worker fails

---

### Phase 3: Performance & Concurrency (Week 6)

**Goal**: Validate performance targets with E2E tests

**E2E Performance Test:**
```cpp
TEST(E2E_Performance, ThousandMessagesPerSecond) {
    ThreadPool pool{8};
    MessageBus bus;

    auto coordinator = std::make_unique<CoordinatorAgent>();
    std::vector<std::unique_ptr<WorkerAgent>> workers;

    for (int i = 0; i < 10; ++i) {
        workers.push_back(std::make_unique<WorkerAgent>());
        bus.registerAgent(workers.back().get());
    }

    // Start all agents
    pool.submit([&]() { coordinator->run(); });
    for (auto& worker : workers) {
        pool.submit([&worker]() { worker->run(); });
    }

    // Send 1000 tasks
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::future<std::string>> results;
    for (int i = 0; i < 1000; ++i) {
        results.push_back(coordinator->processGoal("Task " + std::to_string(i)));
    }

    // Wait for all results
    for (auto& future : results) {
        future.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should process 1000 tasks in under 1 second
    EXPECT_LT(duration_ms.count(), 1000);

    // Calculate throughput
    double throughput = 1000.0 / (duration_ms.count() / 1000.0);
    EXPECT_GT(throughput, 1000.0);  // >1000 tasks/second
}
```

**TDD Workflow:**

**Iteration 1: Benchmark Current Performance**
1. RED: Test fails (too slow)
2. GREEN: Identify bottlenecks with profiling
3. REFACTOR: Optimize hot paths

**Iteration 2: Lock-Free Optimizations**
1. RED: Contention detected
2. GREEN: Improve queue implementation
3. REFACTOR: Validate with ThreadSanitizer

**Iteration 3: Coroutine Overhead**
1. RED: Coroutine suspend/resume too expensive
2. GREEN: Optimize awaitable implementation
3. REFACTOR: Use custom allocators

**Deliverables Week 6:**
- ✅ E2E performance test suite
- ✅ Throughput >1000 tasks/second
- ✅ Latency <10ms p99
- ✅ ThreadSanitizer clean
- ✅ Performance profiling report

---

### Phase 4: External Integration (Weeks 7-8)

**Goal**: Worker can call external AI services (gRPC)

**E2E Test Scenario:**
```cpp
TEST(E2E_AIIntegration, WorkerCallsExternalAIService) {
    // Start mock AI service
    MockAIService mock_service("localhost:50051");
    mock_service.setResponse("Hello", "Hi there!");
    mock_service.start();

    // Setup agents
    ThreadPool pool{4};
    MessageBus bus;

    auto coordinator = std::make_unique<CoordinatorAgent>();
    auto worker = std::make_unique<WorkerAgent>();
    worker->setAIServiceEndpoint("localhost:50051");

    // Start
    pool.submit([&]() { coordinator->run(); });
    pool.submit([&]() { worker->run(); });

    // Send goal that requires AI
    std::string goal = "Generate text: Hello";
    auto result = coordinator->processGoal(goal);

    // Verify AI service was called
    EXPECT_EQ(result.get(), "AI Response: Hi there!");
    EXPECT_EQ(mock_service.getCallCount(), 1);

    mock_service.stop();
}
```

**TDD Workflow:**

**Iteration 1: Mock gRPC Service**
1. RED: No AI service exists
2. GREEN: Create mock gRPC server for testing
3. REFACTOR: Make configurable and reusable

**Iteration 2: Async gRPC Client**
1. RED: Worker can't call gRPC
2. GREEN: Add gRPC client to Worker
3. REFACTOR: Integrate with coroutines

**Iteration 3: Serialization Gateway**
1. RED: Need Cista ↔ Protobuf conversion
2. GREEN: Implement conversion in Worker
3. REFACTOR: Abstract behind interface

**Deliverables Weeks 7-8:**
- ✅ Mock gRPC AI service for testing
- ✅ Async gRPC client in Worker
- ✅ Serialization gateway (Cista ↔ Protobuf)
- ✅ E2E test: Worker calls mock AI service
- ✅ E2E test: Timeout handling for slow AI
- ✅ E2E test: Error handling for AI failures

---

### Phase 5: Three-Layer Hierarchy (Weeks 9-11)

**Goal**: Expand to full Root → Branch → Leaf architecture

**E2E Test Scenario:**
```cpp
TEST(E2E_ThreeLayer, ComplexTaskDecomposition) {
    ThreadPool pool{16};
    MessageBus bus;

    // Create hierarchy
    auto root = std::make_unique<RootAgent>();

    std::vector<std::unique_ptr<BranchAgent>> branches;
    for (int i = 0; i < 3; ++i) {
        branches.push_back(std::make_unique<BranchAgent>());
    }

    std::vector<std::unique_ptr<LeafAgent>> leaves;
    for (int i = 0; i < 9; ++i) {  // 3 leaves per branch
        leaves.push_back(std::make_unique<LeafAgent>());
    }

    // Connect hierarchy
    root->addBranches(branches);
    for (int i = 0; i < branches.size(); ++i) {
        branches[i]->addLeaves({leaves[i*3], leaves[i*3+1], leaves[i*3+2]});
    }

    // Start all agents
    // ... (start agents in pools)

    // Complex goal requiring multi-level decomposition
    std::string goal = "Analyze dataset: [data1, data2, data3] with models: [model_a, model_b, model_c]";

    auto result = root->processGoal(goal);

    // Verify hierarchical execution
    auto trace = root->getExecutionTrace();
    EXPECT_GT(trace.size(), 10);  // Multiple steps

    // Verify all branches participated
    for (auto& branch : branches) {
        EXPECT_GT(branch->getTasksProcessed(), 0);
    }

    // Verify all leaves participated
    for (auto& leaf : leaves) {
        EXPECT_GT(leaf->getTasksProcessed(), 0);
    }
}
```

**Note**: This phase reuses everything built in Phases 1-4. We're just adding the Branch layer and renaming Coordinator→Root, Worker→Leaf.

**Deliverables Weeks 9-11:**
- ✅ `RootAgent` (evolved from CoordinatorAgent)
- ✅ `BranchAgent` (new middle layer)
- ✅ `LeafAgent` (evolved from WorkerAgent)
- ✅ E2E test: 3-layer task decomposition
- ✅ E2E test: Result aggregation up the tree
- ✅ E2E test: Failure recovery at each layer

---

### Phase 6: Robustness & Production (Weeks 12-14)

**E2E Chaos Test:**
```cpp
TEST(E2E_Chaos, SystemRemainsStableUnderFailures) {
    // Setup full hierarchy
    auto system = createFullHierarchy(/*root=*/1, /*branches=*/5, /*leaves=*/20);
    system.start();

    // Run workload while injecting failures
    std::atomic<int> completed{0};
    std::thread workload([&]() {
        for (int i = 0; i < 1000; ++i) {
            auto result = system.root()->processGoal("Task " + std::to_string(i));
            if (result.valid()) ++completed;
        }
    });

    std::thread chaos([&]() {
        std::this_thread::sleep_for(100ms);
        system.killRandomLeaf();  // Simulate leaf crash

        std::this_thread::sleep_for(100ms);
        system.injectNetworkDelay(500ms);  // Simulate network issue

        std::this_thread::sleep_for(100ms);
        system.killRandomBranch();  // Simulate branch crash
    });

    workload.join();
    chaos.join();

    // Verify system recovered and completed most tasks
    EXPECT_GT(completed.load(), 900);  // >90% success despite chaos
    EXPECT_TRUE(system.root()->isHealthy());
}
```

**Deliverables Weeks 12-14:**
- ✅ Graceful shutdown E2E test
- ✅ Failure recovery E2E test
- ✅ Chaos testing E2E suite
- ✅ 24-hour stability E2E test
- ✅ Production deployment guide

---

## TDD Development Workflow

### Daily TDD Cycle

```
Morning:
1. Review failing E2E tests from yesterday
2. Pick ONE test to make pass today
3. Write detailed test scenario

Development:
4. Run test (RED) - verify it fails
5. Write minimal code to pass (GREEN)
6. Run test - verify it passes
7. Refactor code (REFACTOR)
8. Run test - verify still passes
9. Commit

Evening:
10. Write next E2E test for tomorrow
11. Commit failing test
12. Document what needs to be implemented
```

### Git Workflow

```bash
# Feature branch per E2E test
git checkout -b e2e/coordinator-worker-delegation

# Commit the failing test first
git add tests/e2e/DelegationTest.cpp
git commit -m "RED: E2E test for coordinator-worker delegation"

# Make it pass
# ... implement ...
git add src/
git commit -m "GREEN: Implement coordinator-worker delegation"

# Refactor
git add src/
git commit -m "REFACTOR: Clean up delegation logic"

# Merge when E2E test passes
git checkout main
git merge e2e/coordinator-worker-delegation
```

## Success Criteria (E2E Test-Based)

### Week 2
- [ ] E2E Test: Two agents send messages bidirectionally

### Week 3
- [ ] E2E Test: Async message receiving with coroutines

### Week 5
- [ ] E2E Test: Coordinator delegates to 3 workers, aggregates results

### Week 6
- [ ] E2E Test: 1000 tasks/second throughput

### Week 8
- [ ] E2E Test: Worker calls external AI service

### Week 11
- [ ] E2E Test: 3-layer hierarchy processes complex goal

### Week 14
- [ ] E2E Test: 24-hour stability with chaos injection

## Documentation

Each E2E test serves as **living documentation**:

```cpp
/**
 * E2E Test: Coordinator-Worker Delegation
 *
 * Scenario: Coordinator receives a computational goal, delegates to Worker,
 *           and returns aggregated result.
 *
 * Flow:
 *   1. User sends goal to Coordinator
 *   2. Coordinator decomposes goal into task
 *   3. Coordinator sends task message to Worker
 *   4. Worker executes task
 *   5. Worker sends result message to Coordinator
 *   6. Coordinator aggregates and returns to user
 *
 * Performance: Should complete in <10ms for simple tasks
 *
 * Related: architecture.md - Section 2.1 (Task Decomposition)
 */
TEST(E2E_Delegation, CoordinatorDelegatesAndAggregates) {
    // ...
}
```

---

**Document Version**: 2.0 (TDD-Focused)
**Last Updated**: 2025-11-17
**Replaces**: Original phases.md (unit-test-first approach)
