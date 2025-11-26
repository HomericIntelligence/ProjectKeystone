# ProjectKeystone Implementation Plan

## Executive Summary

ProjectKeystone is a high-performance C++20 Hierarchical Multi-Agent System (HMAS) designed for orchestrating specialized AI agents in complex, enterprise-grade workloads. The system implements an actor-model architecture with **four distinct hierarchical layers** (L0: Chief Architect, L1: Component Lead, L2: Module Lead, L3: Task Agent), leveraging modern C++20 features including coroutines, modules, and advanced concurrency primitives.

## 🚀 Development Approach: TDD with E2E Testing + 4-Layer Architecture

**This project follows strict Test-Driven Development (TDD) with End-to-End (E2E) tests as the primary validation method.**

We build incrementally, starting with a **minimal 2-agent system** (Level 0 + Level 3) and progressively adding intermediate layers until we reach the full **4-layer hierarchy**:

- **Level 0**: Chief Architect Agent - Strategic decisions, system-wide coordination
- **Level 1**: Component Lead Agent - Component-level architecture, module coordination
- **Level 2**: Module Lead Agent - Task decomposition, result synthesis, code review
- **Level 3**: Task Agent - Concrete execution, code implementation, testing

This architecture mirrors the agent development structure used to build the system itself, creating a self-similar, fractal-like organization.

👉 **See [TDD_FOUR_LAYER_ROADMAP.md](TDD_FOUR_LAYER_ROADMAP.md) for the complete implementation plan**

## Vision

Build a production-ready, native C++ agent orchestration platform capable of:

- Processing 1M+ messages per second
- Managing hundreds of concurrent specialized agents
- Providing sub-millisecond internal communication latency
- Ensuring graceful degradation and failure recovery
- Scaling horizontally across distributed systems

## Documentation Structure

### 🔴 Start Here

1. **[TDD_FOUR_LAYER_ROADMAP.md](TDD_FOUR_LAYER_ROADMAP.md)** - **READ THIS FIRST!** Complete TDD roadmap with E2E tests
2. **[FOUR_LAYER_ARCHITECTURE.md](FOUR_LAYER_ARCHITECTURE.md)** - 4-layer system architecture specification
3. **README.md** (this file) - Overview and quick start

### 📘 Supporting Documentation

4. **[modules.md](modules.md)** - C++20 module structure and dependencies
5. **[build-system.md](build-system.md)** - CMake configuration and toolchain setup
6. **[testing-strategy.md](testing-strategy.md)** - Testing frameworks and tools (GoogleTest, benchmarks)
7. **[risks.md](risks.md)** - Risk analysis and mitigation strategies
8. **[MIGRATION_ARCHITECTURE_C2_C3.md](MIGRATION_ARCHITECTURE_C2_C3.md)** - C2/C3 architecture migration guide

### 🏗️ Architecture Decision Records (ADRs)

- **[adr/](adr/)** - Architecture Decision Records documenting key design choices
  - ADR-001: MessageBus Architecture
  - ADR-002: Work-Stealing Scheduler Architecture
  - ADR-003: Async Agent Unification
  - ADR-010: Architecture Issue Resolution
  - ADR-011: Phase 6 Architecture Review Fixes
  - ADR-012: CPack Build System Decisions

## Project Directory Structure

```
ProjectKeystone/
├── docs/
│   ├── plan/                    # This implementation plan
│   ├── architecture/            # Architectural Decision Records (ADRs)
│   ├── api/                     # API documentation
│   └── guides/                  # User and developer guides
├── modules/
│   ├── Keystone.Core/           # Core infrastructure
│   │   ├── concurrency/         # Thread pool, coroutines, synchronization
│   │   ├── messaging/           # Message bus, queues, routing
│   │   └── utilities/           # Common utilities
│   ├── Keystone.Protocol/       # Communication protocols
│   │   ├── kim/                 # Keystone Inter-Agent Message
│   │   ├── serialization/       # Cista, FlatBuffers integration
│   │   └── grpc/                # gRPC protocol definitions
│   ├── Keystone.Agents/         # Agent implementations
│   │   ├── base/                # AgentBase and common infrastructure
│   │   ├── root/                # Root (L1) agents
│   │   ├── branch/              # Branch (L2) agents
│   │   └── leaf/                # Leaf (L3) agents
│   └── Keystone.Integration/    # External integrations
│       ├── onnx/                # ONNX Runtime integration
│       ├── grpc-client/         # gRPC client implementations
│       └── monitoring/          # APM and metrics
├── tests/
│   ├── unit/                    # Unit tests per module
│   ├── integration/             # Cross-module integration tests
│   ├── performance/             # Performance benchmarks
│   └── stress/                  # Stress and chaos testing
├── examples/
│   ├── basic/                   # Simple usage examples
│   ├── advanced/                # Complex workflow examples
│   └── benchmarks/              # Performance demonstration
├── tools/
│   ├── generators/              # Code generation tools
│   ├── analyzers/               # Static analysis scripts
│   └── profilers/               # Performance profiling tools
├── third_party/                 # External dependencies
├── cmake/                       # CMake modules and scripts
├── .github/
│   └── workflows/               # CI/CD pipeline definitions
├── CMakeLists.txt               # Root CMake configuration
└── vcpkg.json                   # Dependency manifest
```

## TDD Implementation Roadmap (Revised)

### 🎯 Core Principle: E2E Tests Drive Development

Every phase begins with writing **failing E2E tests** that define the desired behavior, then implementing the minimal code to make them pass.

### Phase 0: E2E Test Infrastructure (Week 1)

**Goal**: Set up the ability to write and run E2E tests

**First E2E Test (Even Though Nothing Exists)**:

```cpp
TEST(E2E_Foundation, CanCreateAndRunTwoAgents) {
    ThreadPool pool{2};
    auto agent1 = std::make_unique<MinimalAgent>("agent1");
    auto agent2 = std::make_unique<MinimalAgent>("agent2");

    agent1->initialize();
    agent2->initialize();

    EXPECT_TRUE(agent1->isRunning());
    EXPECT_TRUE(agent2->isRunning());
}
```

**Deliverables**:

- ✅ E2E test framework (GoogleTest)
- ✅ CMake builds and runs E2E tests
- ✅ First test passes (minimal agent stub)
- ✅ CI runs E2E tests on every commit

### Phase 1: Core Message Passing (Weeks 2-3)

**Goal**: Two agents can send messages to each other

**E2E Test First**:

```cpp
TEST(E2E_MessagePassing, AgentCanSendMessageToAnotherAgent) {
    ThreadPool pool{2};
    MessageBus bus;
    auto sender = std::make_unique<TestAgent>("sender");
    auto receiver = std::make_unique<TestAgent>("receiver");

    sender->send(msg);
    auto received = receiver->waitForMessage(1s);

    EXPECT_EQ(received->payload, "Hello");
}
```

**Deliverables**:

- ✅ KeystoneMessage structure
- ✅ MessageQueue with concurrentqueue
- ✅ MessageBus routing
- ✅ Custom awaitable for async receiving
- ✅ **E2E test: Bidirectional agent communication**

### Phase 2: Coordinator-Worker Pattern (Weeks 4-5)

**Goal**: Coordinator delegates task to Worker, receives result

**E2E Test First**:

```cpp
TEST(E2E_Delegation, CoordinatorDelegatesAndAggregates) {
    auto coordinator = std::make_unique<CoordinatorAgent>();
    auto worker = std::make_unique<WorkerAgent>();

    auto result = coordinator->processGoal("Calculate: 2+2");

    EXPECT_EQ(result.get(), "Result: 4");
}
```

**Deliverables**:

- ✅ CoordinatorAgent with task decomposition
- ✅ WorkerAgent with execution
- ✅ Agent state machines
- ✅ **E2E test: Complete task delegation workflow**

### Phase 3: Performance & Concurrency (Week 6)

**Goal**: Validate performance targets with E2E tests

**E2E Test First**:

```cpp
TEST(E2E_Performance, ThousandTasksPerSecond) {
    // Send 1000 tasks, verify completes in <1 second
    EXPECT_LT(duration_ms, 1000);
}
```

**Deliverables**:

- ✅ Performance optimizations
- ✅ ThreadSanitizer validation
- ✅ **E2E test: >1000 tasks/second**

### Phase 4: External Integration (Weeks 7-8)

**Goal**: Worker can call external AI services (gRPC)

**E2E Test First**:

```cpp
TEST(E2E_AIIntegration, WorkerCallsExternalAIService) {
    MockAIService mock_service("localhost:50051");
    auto worker = std::make_unique<WorkerAgent>();
    worker->setAIServiceEndpoint("localhost:50051");

    auto result = coordinator->processGoal("Generate: Hello");

    EXPECT_EQ(result.get(), "AI Response: Hi there!");
}
```

**Deliverables**:

- ✅ Mock gRPC AI service
- ✅ Async gRPC client
- ✅ Serialization gateway (Cista ↔ Protobuf)
- ✅ **E2E test: Worker calls external AI**

### Phase 5: Three-Layer Hierarchy (Weeks 9-11)

**Goal**: Expand to full Root → Branch → Leaf

**E2E Test First**:

```cpp
TEST(E2E_ThreeLayer, ComplexTaskDecomposition) {
    auto root = createFullHierarchy(/*branches=*/3, /*leaves=*/9);

    auto result = root->processGoal("Complex multi-step task");

    // Verify all layers participated
    EXPECT_GT(root->getTasksProcessed(), 0);
    EXPECT_GT(branches[0]->getTasksProcessed(), 0);
    EXPECT_GT(leaves[0]->getTasksProcessed(), 0);
}
```

**Deliverables**:

- ✅ RootAgent (evolved from Coordinator)
- ✅ BranchAgent (new middle layer)
- ✅ LeafAgent (evolved from Worker)
- ✅ **E2E test: 3-layer hierarchy works**

### Phase 6: Production Hardening (Weeks 12-14)

**Goal**: Chaos testing and production readiness

**E2E Test First**:

```cpp
TEST(E2E_Chaos, SystemRemainsStableUnderFailures) {
    auto system = createFullHierarchy();

    // Inject random failures while processing 1000 tasks
    injectChaos(system);
    auto completed = processTasksBatch(1000);

    EXPECT_GT(completed, 900);  // >90% success despite chaos
}
```

**Deliverables**:

- ✅ Graceful shutdown
- ✅ Failure recovery
- ✅ **E2E test: 24-hour stability**
- ✅ **E2E test: Chaos resilience**

## Technology Stack

### Core Technologies

- **Language**: C++20 (GCC 13+, Clang 16+, or MSVC 2022+)
- **Build System**: CMake 3.28+
- **Package Manager**: vcpkg or Conan
- **Version Control**: Git

### Key Libraries

- **Concurrency**: `concurrentqueue` (lock-free queue)
- **Serialization**: Cista (zero-copy), FlatBuffers
- **RPC**: gRPC with Protocol Buffers
- **AI Integration**: ONNX Runtime
- **Testing**: GoogleTest, Google Benchmark
- **Logging**: spdlog
- **Monitoring**: Prometheus C++ client

### Development Tools

- **Static Analysis**: clang-tidy, cppcheck
- **Formatting**: clang-format
- **Profiling**: perf, Valgrind, Tracy
- **Documentation**: Doxygen, Sphinx

## Success Criteria

### Performance Targets

- **Throughput**: 1M+ messages/second on standard hardware
- **Latency**: <1ms p99 internal message delivery
- **Scalability**: Linear scaling to 1000+ concurrent agents
- **Memory**: <100MB overhead for core framework

### Quality Targets

- **Test Coverage**: >95% for Keystone.Core, >90% for agents
- **Build Time**: <5 minutes for full rebuild
- **Documentation**: 100% API coverage
- **Static Analysis**: Zero critical issues

### Reliability Targets

- **Availability**: Graceful shutdown with zero data loss
- **Recovery**: <100ms failure detection and recovery
- **Isolation**: Complete session context separation
- **Supervision**: Automatic recovery from agent failures

## Team Requirements

### Core Team

- **Senior C++ Engineer** (Lead) - 1 FTE
  - C++20 expertise, coroutines, concurrency
  - System architecture and design

- **C++ Engineers** - 2-3 FTEs
  - Modern C++ development
  - Testing and optimization

- **DevOps Engineer** - 0.5 FTE
  - CI/CD, build systems
  - Deployment and monitoring

### Specialized Skills Needed

- C++20 modules and coroutines
- High-performance concurrent systems
- Actor-model architecture
- gRPC and Protocol Buffers
- ONNX Runtime integration
- Performance profiling and optimization

## Timeline Summary

- **Total Duration**: 20 weeks (5 months)
- **Phase 0-1**: Infrastructure (5 weeks)
- **Phase 2-3**: Agent Framework (6 weeks)
- **Phase 4-5**: Integration (5 weeks)
- **Phase 6-7**: Hardening (4 weeks)

## Next Steps

1. **Immediate Actions**:
   - Review and approve this implementation plan
   - Assemble the development team
   - Set up development environment
   - Create project repository structure

2. **Week 1 Priorities**:
   - Initialize CMake build system
   - Configure CI/CD pipeline
   - Set up dependency management
   - Create coding standards document

3. **Key Decisions Made**:
   - ✅ Testing framework: Google Test (GTest)
   - ✅ Logging library: spdlog
   - Future decisions needed:
     - Package manager (vcpkg vs Conan)
     - Monitoring solution (Prometheus vs alternatives)

## References

- [Architecture Details](architecture.md)
- [Implementation Phases](phases.md)
- [Module Structure](modules.md)
- [Build System](build-system.md)
- [Testing Strategy](testing-strategy.md)
- [Risk Analysis](risks.md)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-17
**Status**: Draft - Pending Approval
