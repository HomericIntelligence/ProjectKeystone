# ProjectKeystone - Hierarchical Multi-Agent System (HMAS)

## Project Overview

**ProjectKeystone** is a C++20-based Hierarchical Multi-Agent System implementing a
4-layer architecture where agents coordinate to accomplish complex tasks through
message passing and delegation.

## Language and Technology Stack

### Primary Language: C++20

**This project is EXCLUSIVELY C++20. Do NOT use Python, Mojo, or other languages for implementation.**

#### Required Technologies

- **C++20** with modules support
- **CMake** 3.20+ for build system
- **Google Test (GTest)** for unit and E2E testing
- **C++20 Coroutines** (`co_await`, `co_return`) for async operations
- **concurrentqueue** library for lock-free message queuing
- **ThreadPool** for parallel agent execution
- **Sanitizers** (ASan, UBSan, TSan) for runtime error detection

#### Language Features Used

- C++20 Coroutines (`std::coroutine_handle`, `std::suspend_always`)
- C++20 Concepts for type constraints
- C++20 Modules (when compiler support available)
- Smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- Move semantics and perfect forwarding
- RAII for resource management

### Build System

Use CMake with:

```cmake
cmake_minimum_required(VERSION 3.20)
project(ProjectKeystone CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

## Architecture

### 4-Layer Hierarchy

ProjectKeystone implements a **4-layer agent hierarchy** based on the Test-Driven Development (TDD) roadmap:

```
Level 0: ChiefArchitectAgent
    │
    ├─── Level 1: ComponentLeadAgent (multiple components)
    │        │
    │        └─── Level 2: ModuleLeadAgent (multiple modules)
    │                 │
    │                 └─── Level 3: TaskAgent (multiple tasks)
```

### Layer Responsibilities

| Level | Agent Type | Responsibility | Analogy |
|-------|-----------|----------------|---------|
| **L0** | ChiefArchitectAgent | Strategic decisions, system-wide coordination | CEO/CTO |
| **L1** | ComponentLeadAgent | Component architecture, module coordination | VP Engineering |
| **L2** | ModuleLeadAgent | Module design, task decomposition | Tech Lead |
| **L3** | TaskAgent | Concrete execution, code implementation | Individual Contributor |

### Development Phases (TDD Approach)

**Phase 1** (Weeks 1-3): L0 ↔ L3 (2 agents)

- Simplest hierarchy: ChiefArchitect delegates directly to TaskAgent
- Validates core message passing

**Phase 2** (Weeks 4-6): L0 ↔ L2 ↔ L3 (3 layers)

- Add ModuleLead for task coordination and result synthesis

**Phase 3** (Weeks 7-9): L0 ↔ L1 ↔ L2 ↔ L3 (4 layers, single component)

- Add ComponentLead for module coordination

**Phase 4** (Weeks 10-12): Full system (multiple components)

- Complete 4-layer hierarchy with parallel execution

**Phase 5** (Weeks 13-14): Performance & robustness

- Scale testing, chaos engineering

See [TDD_FOUR_LAYER_ROADMAP.md](docs/plan/TDD_FOUR_LAYER_ROADMAP.md) for detailed phase descriptions.

## Core Components

### Message Protocol (KIM - Keystone Interchange Message)

```cpp
struct KeystoneMessage {
    std::string msg_id;
    std::string sender_id;
    std::string receiver_id;
    std::string command;
    std::optional<json> payload;
    std::chrono::system_clock::time_point timestamp;
};
```

### Message Bus

**MessageBus** is the central routing hub for all agent communication (introduced in Phase 1):

```cpp
class MessageBus {
public:
    void registerAgent(const std::string& agent_id, BaseAgent* agent);
    void unregisterAgent(const std::string& agent_id);
    bool routeMessage(const KeystoneMessage& msg);
    bool hasAgent(const std::string& agent_id) const;
    std::vector<std::string> listAgents() const;
};
```

**Key Features**:

- **Decoupled Communication**: Agents send messages by agent ID, not direct pointers
- **Dynamic Discovery**: Agents can be registered/unregistered at runtime
- **Thread-Safe**: Mutex-protected agent registry
- **Synchronous Routing**: Phase 1 uses synchronous delivery (async in Phase 2+)
- **Automatic Response Routing**: Messages automatically routed back to sender

**Usage**:

```cpp
// Setup
auto bus = std::make_unique<MessageBus>();
bus->registerAgent(chief->getAgentId(), chief.get());
bus->registerAgent(task->getAgentId(), task.get());
chief->setMessageBus(bus.get());
task->setMessageBus(bus.get());

// Send message (MessageBus routes it)
auto msg = KeystoneMessage::create("chief", "task", "echo hello");
chief->sendMessage(msg);  // MessageBus handles routing
```

See [ADR-001](docs/plan/adr/ADR-001-message-bus-architecture.md) for architecture details.

### Agent Base Class

```cpp
class BaseAgent {
public:
    virtual Response processMessage(const KeystoneMessage& msg) = 0;
    void sendMessage(const KeystoneMessage& msg);
    void receiveMessage(const KeystoneMessage& msg);
    void setMessageBus(MessageBus* bus);

protected:
    std::string agent_id_;
    MessageBus* message_bus_;

private:
    std::queue<KeystoneMessage> inbox_;
    std::mutex inbox_mutex_;
};
```

**Phase 1 Design**: Synchronous message processing with MessageBus routing

### Concurrency Model

- **ThreadPool**: Fixed-size pool for agent execution
- **MessageQueue**: Lock-free concurrent queue (`concurrentqueue`)
- **Coroutines**: C++20 coroutines for async message processing
- **Task<T>**: Custom awaitable type for async operations

## Project Structure

```
ProjectKeystone/
├── CMakeLists.txt           # Root CMake configuration
├── CLAUDE.md                # This file - project overview for Claude
├── README.md                # Project README
├── docs/
│   └── plan/                # Architecture and planning docs
│       ├── FOUR_LAYER_ARCHITECTURE.md
│       ├── TDD_FOUR_LAYER_ROADMAP.md
│       ├── build-system.md
│       ├── testing-strategy.md
│       └── modules.md
├── include/                 # Public headers
│   ├── agents/
│   │   ├── base_agent.hpp
│   │   ├── chief_architect_agent.hpp
│   │   ├── component_lead_agent.hpp
│   │   ├── module_lead_agent.hpp
│   │   └── task_agent.hpp
│   ├── core/
│   │   ├── message.hpp
│   │   ├── message_bus.hpp
│   │   └── message_queue.hpp
│   └── concurrency/
│       ├── thread_pool.hpp
│       ├── task.hpp
│       └── coroutine_helpers.hpp
├── src/                     # Implementation files
│   ├── agents/              # C++ agent implementations
│   ├── core/                # C++ core components
│   ├── concurrency/         # C++ concurrency primitives
│   └── keystone/            # Python DAG orchestration daemon
│       ├── __init__.py
│       ├── __main__.py      # python -m keystone entry point
│       ├── config.py        # Lazy settings singleton
│       ├── daemon.py        # Main daemon with NATS + health
│       ├── dag_walker.py    # DAG traversal and task scheduling
│       ├── health.py        # HTTP health check endpoint
│       ├── logging.py       # Structured JSON logging
│       ├── maestro_client.py # ai-maestro API client with retries
│       ├── models.py        # Task/Agent data models
│       └── nats_listener.py # NATS JetStream event listener
├── tests/                   # Test files
│   ├── unit/                # C++ unit tests (Google Test)
│   ├── e2e/                 # C++ E2E tests
│   │   └── phase1/
│   ├── conftest.py          # Python shared fixtures
│   ├── helpers.py           # Python shared test helpers
│   ├── test_config.py       # Settings lazy loading tests
│   ├── test_dag_walker.py   # DAG traversal tests
│   ├── test_health.py       # Health endpoint tests
│   ├── test_maestro_client.py # API client parsing tests
│   ├── test_nats_listener.py  # NATS listener tests
│   └── test_task_claimer.py # Task assignment tests
└── third_party/             # External dependencies
    ├── concurrentqueue/
    └── googletest/
```

## Development Workflow

### TDD Approach

1. **Write E2E test first** (RED)
2. **Implement minimal code** to pass test (GREEN)
3. **Refactor** while keeping tests green
4. **Commit** when tests pass

### Test Naming Convention

```cpp
// E2E tests (Google Test)
TEST(E2E_Phase1, ChiefArchitectDelegatesToTaskAgent) {
    // Test implementation
}

TEST(E2E_Phase2, ModuleLeadSynthesizesTaskResults) {
    // Test implementation
}

// Unit tests (Google Test)
TEST(MessageQueueTest, CanPushAndPopMessages) {
    // Test implementation
}

TEST(ThreadPoolTest, SubmitsAndExecutesTasks) {
    // Test implementation
}
```

### Git Workflow

### ⚠️ MANDATORY: Branch-First Development

**CRITICAL RULE: NEVER commit directly to `main` branch**

All code changes MUST use the Pull Request workflow:

#### 1. Create Feature Branch FIRST

```bash
# Create timestamped feature branch
git checkout -b feat/descriptive-name-$(date +%Y%m%d-%H%M%S)

# For fixes
git checkout -b fix/issue-description-$(date +%Y%m%d-%H%M%S)
```

#### 2. Make Changes & Commit

```bash
git add <files>
git commit -m "feat: descriptive commit message"
git push -u origin feat/descriptive-name-...
```

#### 3. Create Pull Request

```bash
gh pr create --title "feat: Brief description" \
             --body "## Summary
- Change 1
- Change 2

## Testing
- All tests pass (466/466)
- No ASan/UBSan warnings

🤖 Generated with Claude Code"
```

#### 4. Wait for Review & Merge

- Do NOT merge your own PR
- Let CI/CD or user merge after review

#### Branch Naming Convention

- **Features**: `feat/short-description-YYYYMMDD-HHMMSS`
- **Fixes**: `fix/issue-description-YYYYMMDD-HHMMSS`
- **Refactors**: `refactor/component-name-YYYYMMDD-HHMMSS`
- **Tests**: `test/test-description-YYYYMMDD-HHMMSS`
- **Docs**: `docs/doc-description-YYYYMMDD-HHMMSS`

#### Commit Message Format

- `feat:` - New feature
- `fix:` - Bug fix
- `refactor:` - Code refactoring
- `test:` - Test changes
- `docs:` - Documentation changes
- `chore:` - Maintenance tasks

#### What NOT To Do

❌ **NEVER** `git checkout main` then commit
❌ **NEVER** `git push origin main` directly
❌ **NEVER** commit without being on a feature branch
❌ **NEVER** skip creating a PR
❌ **NEVER** merge your own PR without approval

#### Pre-Commit Checklist

Before every commit:

1. ✅ On feature branch: `git branch --show-current`
2. ✅ All tests pass: `make test.debug.asan.native`
3. ✅ Code formatted: `make format.native`
4. ✅ No compilation warnings
5. ✅ Changes are focused and atomic

## Agent Configuration

### Agent Files Location

`.claude/agents/` - Agent configuration files for Claude Code

### Current Agents (ProjectKeystone-specific)

These replace the ml-odyssey agents:

- **chief-architect.md** - Level 0 strategic orchestrator (C++20 HMAS)
- **component-lead.md** - Level 1 component coordinator
- **module-lead.md** - Level 2 module coordinator
- **task-agent.md** - Level 3 concrete executor
- **implementation-engineer.md** - C++20 implementation specialist
- **test-engineer.md** - Google Test specialist

### Agent Responsibilities

#### Chief Architect (Level 0)

- Strategic decisions for HMAS
- Select which components to build
- Coordinate across all levels
- C++20 architecture decisions

#### Component Lead (Level 1)

- Component-level architecture
- Module coordination
- Cross-module dependencies

#### Module Lead (Level 2)

- Module-level design
- Task decomposition
- Result synthesis from tasks

#### Task Agent (Level 3)

- Concrete code implementation in C++20
- Unit test execution
- Result reporting

## Documentation Rules

### Where to Document

**Planning Phase**:

- Architecture decisions → `docs/plan/`
- E2E test specifications → `tests/e2e/README.md`

**Implementation Phase**:

- Code comments (Doxygen style)
- Header documentation
- Test documentation

**Do NOT**:

- Create documentation outside project structure
- Duplicate information across files
- Write verbose READMEs unless requested

## Testing Strategy

### E2E Tests (Priority)

E2E tests drive development. Each phase has specific E2E tests:

**Phase 1**: Basic delegation (L0 → L3)

```cpp
TEST(E2E_Phase1, ChiefArchitectDelegatesToTaskAgent) {
    // Test implementation
}

TEST(E2E_Phase1, ChiefArchitectCoordinatesThreeTaskAgents) {
    // Test implementation
}
```

See [testing-strategy.md](docs/plan/testing-strategy.md) for complete test catalog.

### Sanitizers

Run tests with sanitizers to catch runtime errors:

- **AddressSanitizer (ASan)**: Detects memory errors (use-after-free, buffer overflows)
- **UndefinedBehaviorSanitizer (UBSan)**: Detects undefined behavior
- **ThreadSanitizer (TSan)**: Detects data races and threading issues
- **MemorySanitizer (MSan)**: Detects uninitialized memory reads

Enable in CMake with:

```cmake
# For ASan + UBSan
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer")

# For TSan (separate build, incompatible with ASan)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
```

### Unit Tests

Write unit tests for:

- MessageQueue operations
- ThreadPool functionality
- Message serialization/deserialization
- Coroutine helpers

### Test Execution

**All builds and tests use the Makefile (Docker by default, native mode with `.native` suffix).**

```bash
# Run all tests with sanitizers
make test.debug.asan          # Run all tests with AddressSanitizer (Docker)
make test.debug.tsan          # Run all tests with ThreadSanitizer
make test.debug.ubsan         # Run all tests with UBSan
make test.debug.lsan          # Run all tests with LeakSanitizer
make test.debug.msan          # Run all tests with MemorySanitizer

# Run specific test suites
make test.basic               # Run basic delegation tests
make test.module              # Run module coordination tests
make test.unit                # Run unit tests
make test.component           # Run component coordination tests
make test.async               # Run async delegation tests

# Native mode (faster iteration - runs on host)
make test.debug.asan.native   # Run ASan tests on host
make test.basic.native        # Run basic tests on host

# Show all available commands
make help
```

## Build System

**All development uses the Makefile (Docker by default, native mode with `.native` suffix).**

### Prerequisites

- **GNU Make** - Standard on Linux/macOS
- **Docker** 20.10+ installed (for default Docker mode)
- **docker-compose** 1.29+ (optional but recommended)

### Quick Start with Makefile

```bash
# Show all available commands
make help

# Build with different configurations
make compile.debug            # Build debug mode (Docker)
make compile.release          # Build release mode
make compile.debug.asan       # Build with AddressSanitizer
make compile.debug.tsan       # Build with ThreadSanitizer

# Run all tests
make test.debug.asan          # Run tests with ASan

# Run specific test suites
make test.basic               # Run basic delegation tests
make test.module              # Run module coordination tests

# Native mode (run on host instead of Docker)
make compile.debug.asan.native  # Build with ASan on host
make test.debug.asan.native     # Run tests on host

# Lint and format
make lint                     # Run all linters
make format                   # Format C++ code
make format.check             # Check formatting (CI)
```

### Build Directory Structure

Builds output to `build/x86.<mode>.<sanitizer>/` for parallel builds:

```
build/
├── x86.debug/           # Debug build
├── x86.release/         # Release build
├── x86.debug.asan/      # Debug + AddressSanitizer
├── x86.debug.tsan/      # Debug + ThreadSanitizer
├── x86.debug.ubsan/     # Debug + UBSan
├── x86.debug.lsan/      # Debug + LeakSanitizer
├── x86.debug.grpc/      # Debug + gRPC
└── x86.debug.coverage/  # Debug + Coverage
```

### Manual Docker Commands (if not using Makefile)

```bash
# Development environment
docker-compose up -d dev
docker-compose exec dev bash

# Inside dev container: build and test
cmake -S . -B build/asan -G Ninja \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
cmake --build build/asan
./build/asan/basic_delegation_tests
```

### Docker Commands

#### Build and Test (Production)

```bash
# Build the runtime image and run tests
docker-compose up test

# Or using docker directly
docker build --target runtime -t projectkeystone:latest .
docker run --rm projectkeystone:latest
```

#### Development Environment

```bash
# Start development container with mounted source
docker-compose up -d dev

# Enter the container
docker-compose exec dev bash

# Inside container: build and test
cd build
cmake -G Ninja ..
ninja
./phase1_e2e_tests

# Exit container
exit

# Stop container
docker-compose down
```

#### Clean Build

```bash
# Remove build cache and rebuild
docker-compose down -v
docker-compose build --no-cache
docker-compose up test
```

### Docker Architecture

The project uses a **multi-stage Dockerfile**:

1. **builder** stage - Full build environment (Ubuntu 22.04, GCC 12, CMake, Ninja)
2. **runtime** stage - Minimal runtime (just test executables)
3. **development** stage - Dev tools (gdb, valgrind, clang-format)

### Docker Compose Services

- `test` - Build and run tests (runtime stage)
- `dev` - Development environment with mounted source
- `build` - Build only (for CI/CD)

## Dependencies

### System Requirements

- **Docker** 20.10+
- **docker-compose** 1.29+ (recommended)

### Container Dependencies (Managed by Docker)

All dependencies are handled inside the Docker container:

1. **C++20 Compiler** - GCC 12 with sanitizers support (installed in Dockerfile)
2. **CMake** 3.22+ (installed in Dockerfile)
3. **Google Test (GTest)** - Fetched by CMake via FetchContent
4. **Ninja** - Fast build system (installed in Dockerfile)
5. **Sanitizers** - ASan, UBSan, TSan, MSan (built into GCC 12)

No manual dependency installation required on host!

## Coding Standards

### C++20 Style

```cpp
// Use snake_case for variables and functions
void process_message(const KeystoneMessage& msg);

// Use PascalCase for types
class ChiefArchitectAgent;
struct KeystoneMessage;

// Use trailing return types for complex types
auto createAgent() -> std::unique_ptr<AgentBase>;

// Use coroutines for async operations
Task<void> processMessageAsync(const KeystoneMessage& msg) {
    co_await sendToAgent(msg);
    co_return;
}
```

### Memory Management

- Prefer `std::unique_ptr` for ownership
- Use `std::shared_ptr` only when shared ownership needed
- Use references (`const T&`) for non-owning access
- RAII for all resources

### Error Handling

- Use exceptions for exceptional cases
- Use `std::optional` for nullable returns
- Use `std::expected` (C++23) when available, or custom Result<T, E>

## Success Criteria

### Phase 1 Success

- ✅ ChiefArchitect sends command to TaskAgent
- ✅ TaskAgent executes bash command (add two numbers)
- ✅ TaskAgent returns result to ChiefArchitect
- ✅ MessageBus properly routes all messages
- ✅ All agents decoupled via MessageBus
- ✅ All E2E tests passing (3/3)
- ✅ All unit tests passing (MessageBus tests)
- ✅ No memory leaks (valgrind clean)
- ✅ Thread-safe UUID generation
- ✅ RAII resource management (PipeHandle for popen/pclose)

### Phase 2 Success (L0 ↔ L2 ↔ L3)

- ✅ ModuleLeadAgent decomposes module goals into tasks
- ✅ ModuleLeadAgent coordinates 3 TaskAgents
- ✅ ModuleLeadAgent synthesizes results from all TaskAgents
- ✅ State machine tracking (IDLE → PLANNING → WAITING → SYNTHESIZING → IDLE)
- ✅ ChiefArchitect → ModuleLead → TaskAgents message flow working
- ✅ Variable task count handling (2-3 tasks tested)
- ✅ All E2E tests passing (5/5: 3 Phase1 + 2 Phase2)
- ✅ All unit tests passing (11/11)
- ✅ 16/16 total tests passing (100%)

### Phase 3 Success (L0 ↔ L1 ↔ L2 ↔ L3) - Full 4-Layer Hierarchy

- ✅ ComponentLeadAgent decomposes component goals into modules
- ✅ ComponentLeadAgent coordinates 2 ModuleLeadAgents
- ✅ Each ModuleLead coordinates 3 TaskAgents (6 total)
- ✅ Component-level aggregation of module results
- ✅ State machine tracking (IDLE → PLANNING → WAITING_FOR_MODULES → AGGREGATING)
- ✅ Full 4-layer message flow: Chief → Component → Module → Task
- ✅ All E2E tests passing (6/6: 3 Phase1 + 2 Phase2 + 1 Phase3)
- ✅ All unit tests passing (11/11)
- ✅ 17/17 total tests passing (100%)
- ✅ Complete hierarchical architecture working end-to-end

### Phase 8 Success (Optional: Distributed Multi-Node Communication)

**Status**: ✅ COMPLETE (Optional feature - disabled by default)

Phase 8 adds distributed multi-node communication via gRPC, YAML task specifications, and service discovery.

**Enabled with**: `-DENABLE_GRPC=ON` during CMake configuration

**Core Features**:

- ✅ gRPC-based distributed communication between nodes
- ✅ Protocol Buffers message serialization
- ✅ YAML task specification format (Kubernetes-style)
- ✅ ServiceRegistry for multi-node agent discovery
- ✅ HMASCoordinator service for task routing and tracking
- ✅ Load balancing strategies (ROUND_ROBIN, LEAST_LOADED, RANDOM)
- ✅ Result aggregation (WAIT_ALL, FIRST_SUCCESS, MAJORITY)
- ✅ Heartbeat monitoring (1s interval, 3s timeout)
- ✅ Docker Compose multi-node deployment (7 nodes)

**Testing**:

- ✅ 26 E2E test cases in distributed_grpc_tests
- ✅ YAML parsing and generation
- ✅ ServiceRegistry registration and queries
- ✅ Load balancing verification
- ✅ Result aggregation strategies
- ✅ Integration tests

**Documentation**:

- See [docs/PHASE_8_OPTIONAL_BUILD.md](docs/PHASE_8_OPTIONAL_BUILD.md) for build instructions
- See [docs/PHASE_8_COMPLETE.md](docs/PHASE_8_COMPLETE.md) for architecture
- See [docs/NETWORK_PROTOCOL.md](docs/NETWORK_PROTOCOL.md) for gRPC APIs
- See [docs/YAML_SPECIFICATION.md](docs/YAML_SPECIFICATION.md) for YAML format

**Important Notes**:

- Phase 8 is OPTIONAL and disabled by default
- Base system (Phases 1-7) works without gRPC dependencies
- Agents support both local MessageBus AND distributed gRPC modes
- Conditional compilation using `#ifdef ENABLE_GRPC`

### Overall Success

- All 4 layers implemented (Phases 1-3)
- E2E tests for all phases passing
- Performance benchmarks met (100+ agents)
- Chaos testing passes (20 random failures tolerated)
- Optional distributed features (Phase 8) available

## Common Pitfalls

### ❌ DO NOT

- Use Python or other languages for implementation
- Skip E2E tests
- Implement all layers at once (violates TDD approach)
- Use raw pointers for ownership
- Block threads unnecessarily

### ✅ DO

- Follow TDD: test first, then implement
- Use C++20 coroutines for async operations
- Keep commits focused and tests passing
- Document architectural decisions
- Follow the phase-by-phase roadmap

## Quick Reference

### Current Phase

**Phase 3 COMPLETE**: Full 4-layer hierarchy (L0 ↔ L1 ↔ L2 ↔ L3) ✅
**Phase 8 COMPLETE (Optional)**: Distributed multi-node communication ✅

### Current Achievement

✅ Complete hierarchical agent system with ComponentLead coordinating ModuleLeads and TaskAgents
✅ Optional distributed gRPC features for multi-node deployments

### Quick Commands

**Using Makefile:**

```bash
# Show all available commands
make help

# Build
make compile.debug.asan       # Build with ASan (Docker)
make compile.release          # Build release mode
make compile.debug.tsan       # Build with TSan

# Test
make test.debug.asan          # Run all tests with ASan
make test.basic               # Run basic delegation tests
make test.module              # Run module coordination tests
make test.unit                # Run unit tests

# Lint & Format
make lint                     # Run all linters
make format                   # Format code
make format.check             # Check formatting (CI)

# Benchmarks & Load Tests
make benchmark                # Run all benchmarks
make load-test                # Run load tests
make load-test.quick          # Quick load tests (CI)

# Native Mode (run on host - add .native suffix)
make compile.debug.asan.native
make test.debug.asan.native

# Docker Management
make docker.build             # Build Docker images
make docker.up                # Start dev container
make docker.shell             # Enter dev container
```

**With Phase 8 (Distributed Features):**

```bash
# Build with gRPC support
make compile.debug.grpc
make test.grpc

# Or manually
cmake -S . -B build/grpc -G Ninja -DENABLE_GRPC=ON
cmake --build build/grpc
./build/grpc/distributed_grpc_tests

# Multi-node deployment
docker-compose -f docker-compose-distributed.yaml up
```

See [docs/PHASE_8_OPTIONAL_BUILD.md](docs/PHASE_8_OPTIONAL_BUILD.md) for detailed instructions.

### Current Branch

`claude/first-agent-test-<session-id>`

---

**Last Updated**: 2025-11-21
**Version**: 1.2 (Phase 8 Optional)
**Project**: ProjectKeystone HMAS (C++20)
