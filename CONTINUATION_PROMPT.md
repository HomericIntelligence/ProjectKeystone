**FIXME** - Integrate this file contents into the documentation, agentic framework, and CLAUDE.md and remove this file
# ProjectKeystone HMAS - Continuation Prompt

## Current Status Summary

**Branch**: `main`
**Tests**: 329/329 passing (100%)
**Status**: All Phases 1-5 complete and merged to main
**Code Base**: ~20,335 lines of production code and tests

### Completed Phases

- ✅ **Phase 1**: L0 ↔ L3 (ChiefArchitect → TaskAgent basic delegation)
- ✅ **Phase 2**: L0 ↔ L2 ↔ L3 (Added ModuleLead coordination)
- ✅ **Phase 3**: L0 ↔ L1 ↔ L2 ↔ L3 (Full 4-layer hierarchy)
- ✅ **Phase A**: Async work-stealing architecture (C++20 coroutines)
- ✅ **Phase B**: Async agent migration (all agents async)
- ✅ **Phase C**: Performance & monitoring (metrics, profiling, priorities)
- ✅ **Phase D**: Distributed simulation (NUMA, network, cluster simulation)
- ✅ **Phase 4**: Multi-component coordination (parallel execution, dependencies)
- ✅ **Phase 5**: Chaos engineering (failures, partitions, retries, recovery)

---

## Prompt for Next Session

I'm continuing work on **ProjectKeystone**, a C++20 Hierarchical Multi-Agent System (HMAS).
The project has completed Phases 1-5 with all 329 tests passing on the main branch. Here's
where we are:

### What's Been Completed

1. **4-Layer Async Hierarchy**: ChiefArchitect (L0) → ComponentLead (L1) → ModuleLead (L2) →
   TaskAgent (L3) with C++20 coroutines
2. **Work-Stealing Scheduler**: Lock-free work-stealing queues for optimal CPU utilization
3. **Distributed Simulation**: NUMA-aware cluster simulation with network latency, packet loss, and partitions
4. **Chaos Engineering**: Failure injection, network partitions, retry logic, heartbeat monitoring, circuit breakers
5. **Production Features**: Metrics, profiling, priority queues, deadline scheduling, message pools

### Current Branch

- **Branch**: `main`
- **Status**: Production-ready with all phases merged
- **Tests**: 329/329 passing (100%)

### Project Structure

```
ProjectKeystone/
├── include/
│   ├── agents/          # Async agent hierarchy (Base, Chief, Component, Module, Task)
│   ├── core/            # Message system, metrics, chaos components
│   ├── concurrency/     # Task, ThreadPool, WorkStealingScheduler
│   └── simulation/      # SimulatedCluster, Network, NUMANode
├── src/                 # Implementation files
├── tests/
│   ├── e2e/             # 59 end-to-end tests across all phases
│   └── unit/            # 270 unit tests
└── docs/plan/           # Phase completion docs (PHASE_A-D, 4, 5)
```

---

## Recommended Next Steps

### Option 1: Production Deployment (Phase 6)

**Goal**: Deploy the HMAS to production with containerization and orchestration

**Tasks**:

1. **Docker Multi-Stage Build**
   - Optimize Dockerfile for production (smaller image)
   - Separate build and runtime stages
   - Security hardening (non-root user, minimal base image)

2. **Kubernetes Orchestration**
   - Create K8s deployment manifests
   - StatefulSet for HMAS agents with persistent storage
   - Service mesh integration (Istio/Linkerd)
   - Horizontal Pod Autoscaling based on metrics

3. **Monitoring & Observability**
   - Prometheus metrics exporter (integrate with existing Metrics class)
   - Grafana dashboards for HMAS hierarchy visualization
   - Distributed tracing with OpenTelemetry
   - Log aggregation (ELK stack or Loki)

4. **Health Checks & Readiness Probes**
   - Liveness probes for agent health
   - Readiness probes for MessageBus connectivity
   - Graceful shutdown with signal handling

**Deliverables**:

- Production Dockerfile
- Kubernetes manifests (Deployment, Service, ConfigMap, Secret)
- Prometheus metrics endpoint
- Grafana dashboard JSON
- Helm chart for easy deployment

---

### Option 2: AI Integration (Phase 7)

**Goal**: Integrate LLM-based task agents for natural language processing

**Tasks**:

1. **LLM Integration**
   - Create `LLMTaskAgent` inheriting from `AsyncTaskAgent`
   - Integrate with OpenAI API, Anthropic Claude, or local LLMs (llama.cpp)
   - Prompt engineering for task execution

2. **Natural Language Goal Processing**
   - Enhance `ChiefArchitectAgent` with NL understanding
   - Parse user goals into structured plans
   - Generate task descriptions for LLM agents

3. **Code Generation Pipeline**
   - LLM generates code for tasks
   - Static analysis and validation
   - Automated testing of generated code
   - Feedback loop for corrections

4. **RAG (Retrieval-Augmented Generation)**
   - Vector database integration (ChromaDB, Pinecone)
   - Embed codebase for context retrieval
   - Semantic search for relevant code examples

**Deliverables**:

- `LLMTaskAgent` class with API integration
- Natural language goal parser
- Code generation pipeline with validation
- RAG system for codebase context

---

### Option 3: Real Distributed System (Phase 8)

**Goal**: Convert simulated distributed system to real multi-node deployment

**Tasks**:

1. **gRPC Communication**
   - Replace `SimulatedNetwork` with real gRPC
   - Define `.proto` files for `KeystoneMessage`
   - Implement gRPC client/server for MessageBus

2. **Service Discovery**
   - Integrate with etcd or Consul
   - Dynamic agent registration
   - Health checking and failover

3. **Consensus Algorithm**
   - Implement Raft or Paxos for leader election
   - Distributed state management
   - Log replication for fault tolerance

4. **Multi-Node Deployment**
   - Deploy across multiple physical/virtual machines
   - Cross-datacenter support
   - Network partition handling (real split-brain scenarios)

**Deliverables**:

- gRPC-based MessageBus implementation
- Service discovery integration
- Raft consensus implementation
- Multi-node deployment guide

---

### Option 4: Enhanced Testing & Quality (Phase 9)

**Goal**: Achieve near-100% code coverage and production-grade quality

**Tasks**:

1. **Code Coverage Analysis**
   - Integrate gcov/lcov for coverage reports
   - Achieve 95%+ line coverage
   - Identify untested edge cases

2. **Fuzz Testing**
   - AFL or libFuzzer integration
   - Fuzz message serialization/deserialization
   - Fuzz MessageBus routing logic

3. **Static Analysis**
   - clang-tidy with strict checks
   - cppcheck integration
   - SonarQube analysis

4. **Performance Benchmarking**
   - Google Benchmark integration
   - Latency percentiles (p50, p95, p99, p99.9)
   - Throughput benchmarks (messages/sec)
   - Scalability testing (100, 1000, 10000 agents)

**Deliverables**:

- Coverage reports with 95%+ coverage
- Fuzz testing harness
- Static analysis CI integration
- Performance benchmark suite

---

### Option 5: Advanced Features (Phase 10)

**Goal**: Add advanced distributed system features

**Tasks**:

1. **Message Persistence**
   - Integrate RocksDB or LevelDB
   - Durable message queues
   - Replay capability for debugging

2. **Distributed Tracing**
   - Trace message flow across hierarchy
   - OpenTelemetry integration
   - Jaeger or Zipkin visualization

3. **A/B Testing Framework**
   - Test different agent strategies
   - Metrics comparison
   - Automated rollback on regression

4. **Auto-Scaling**
   - Dynamic agent spawning based on load
   - Resource utilization monitoring
   - Graceful agent termination

**Deliverables**:

- Message persistence layer
- Distributed tracing integration
- A/B testing framework
- Auto-scaling controller

---

## Quick Start Commands

### Run All Tests

```bash
docker-compose up test
# Or manually:
cd build && ctest --output-on-failure
```

### Build with Sanitizers

```bash
docker-compose up test-asan   # AddressSanitizer
docker-compose up test-tsan   # ThreadSanitizer
docker-compose up test-ubsan  # UndefinedBehaviorSanitizer
```

### Create New Feature Branch

```bash
# For Phase 6 (Production Deployment)
git checkout -b claude/phase-6-production-deployment-<session-id>

# For Phase 7 (AI Integration)
git checkout -b claude/phase-7-ai-integration-<session-id>

# For Phase 8 (Real Distributed System)
git checkout -b claude/phase-8-distributed-system-<session-id>
```

---

## Recommended Prompt for Next Session

**If continuing with Phase 6 (Production Deployment)**:

> I'm working on ProjectKeystone HMAS (C++20 hierarchical multi-agent system). Phases 1-5
> are complete and merged to main with 329/329 tests passing. Now I want to prepare for
> production deployment (Phase 6).
>
> Current status:
>
> - Branch: `main` (all phases merged)
> - 329/329 tests passing
> - Production-ready HMAS with async work-stealing, distributed simulation, and chaos engineering
>
> Next steps for Phase 6:
>
> 1. Optimize Dockerfile for production (multi-stage build, security hardening)
> 2. Create Kubernetes deployment manifests (StatefulSet, Service, ConfigMap)
> 3. Add Prometheus metrics exporter (integrate with existing Metrics class)
> 4. Create Grafana dashboard for HMAS hierarchy visualization
>
> Please create a new branch `claude/phase-6-production-deployment-<session-id>` and start
> with optimizing the Dockerfile for production. The current Dockerfile is in the root
> directory.

**If continuing with Phase 7 (AI Integration)**:

> I'm working on ProjectKeystone HMAS (C++20 hierarchical multi-agent system). Phases 1-5
> are complete and merged to main with 329/329 tests passing. Now I want to add LLM
> integration (Phase 7).
>
> Current status:
>
> - Branch: `main` (all phases merged)
> - We have `AsyncTaskAgent` as the base L3 executor
> - Production-ready HMAS with full async hierarchy
>
> Next steps for Phase 7:
>
> 1. Design `LLMTaskAgent` class that inherits from `AsyncTaskAgent`
> 2. Integrate with OpenAI API or Anthropic Claude API
> 3. Implement prompt engineering for task execution
> 4. Add E2E tests for LLM-based task execution
>
> Please create a new branch `claude/phase-7-ai-integration-<session-id>` and start by
> designing the `LLMTaskAgent` class interface and integration points.

**If continuing with Phase 8 (Real Distributed System)**:

> I'm working on ProjectKeystone HMAS (C++20 hierarchical multi-agent system). Phases 1-5
> are complete and merged to main with 329/329 tests passing. Now I want to convert the
> simulated distributed system to a real multi-node deployment (Phase 8).
>
> Current status:
>
> - Branch: `main` (all phases merged)
> - We have `SimulatedNetwork`, `SimulatedCluster`, `SimulatedNUMANode` for testing
> - Now need real gRPC-based communication
>
> Next steps for Phase 8:
>
> 1. Define `.proto` files for `KeystoneMessage` and agent communication
> 2. Implement gRPC client/server for MessageBus
> 3. Replace `SimulatedNetwork` with real gRPC communication
> 4. Add service discovery (etcd or Consul)
>
> Please create a new branch `claude/phase-8-distributed-system-<session-id>` and start by
> designing the `.proto` files for the message protocol.

**If continuing with Phase 9 (Enhanced Testing)**:

> I'm working on ProjectKeystone HMAS (C++20 hierarchical multi-agent system). Phases 1-5
> are complete and merged to main with 329/329 tests passing. Now I want to enhance
> testing and achieve 95%+ code coverage (Phase 9).
>
> Current status:
>
> - Branch: `main` (all phases merged)
> - 329/329 tests passing (59 E2E + 270 unit tests)
> - Need coverage analysis to identify gaps
>
> Next steps for Phase 9:
>
> 1. Integrate gcov/lcov for code coverage reports
> 2. Run coverage analysis and identify untested code paths
> 3. Add tests to reach 95%+ coverage
> 4. Set up coverage reporting in CI/CD
>
> Please create a new branch `claude/phase-9-enhanced-testing-<session-id>` and start by
> adding gcov/lcov integration to CMakeLists.txt.

**If just reviewing/documenting**:

> I'm reviewing ProjectKeystone HMAS (C++20 hierarchical multi-agent system). Phases 1-5
> are complete and merged to main with 329/329 tests passing.
>
> Current status:
>
> - Branch: `main` (production-ready)
> - All phases complete (1-5, A-D)
> - Full documentation in docs/plan/
>
> Please review the current architecture and suggest the best next phase based on the
> TDD_FOUR_LAYER_ROADMAP.md document and completed phases.

---

## Important Files to Reference

- **Architecture**: `docs/plan/FOUR_LAYER_ARCHITECTURE.md`
- **Roadmap**: `docs/plan/TDD_FOUR_LAYER_ROADMAP.md`
- **Phase Completions**: `docs/plan/PHASE_{A,B,C,D,4,5}_COMPLETE.md`
- **Build System**: `docs/plan/build-system.md`
- **Testing Strategy**: `docs/plan/testing-strategy.md`
- **Project Overview**: `CLAUDE.md` (main instructions for Claude)

---

## Key Metrics to Track

- **Test Coverage**: Currently 329/329 (100%), aim to maintain and improve
- **Build Time**: Monitor Docker build times
- **Test Execution Time**: Currently ~39 seconds
- **Code Quality**: No compiler warnings, sanitizer clean
- **Performance**: Message throughput, latency percentiles
- **Memory Safety**: No leaks (valgrind clean)

---

## Development Workflow

1. **Create Feature Branch**: `git checkout -b claude/phase-X-<description>-<session-id>`
2. **Make Changes**: Follow TDD approach (test first, then implement)
3. **Run Tests**: `docker-compose up test` (all tests must pass)
4. **Run Sanitizers**: `docker-compose up test-asan test-ubsan` (must be clean)
5. **Commit**: Use conventional commits (`feat:`, `fix:`, `docs:`, etc.)
6. **Push**: `git push -u origin <branch-name>`
7. **Create PR**: Merge back to main when complete

---

**Last Updated**: 2025-11-19
**Project Status**: Production-ready HMAS with comprehensive testing (Phases 1-5 complete)
**Next Phase**: TBD (choose from Options 1-5 above)
