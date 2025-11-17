# ProjectKeystone - Hierarchical Multi-Agent System (HMAS)

## Project Overview

**ProjectKeystone** is a C++20-based Hierarchical Multi-Agent System implementing a 4-layer architecture where agents coordinate to accomplish complex tasks through message passing and delegation.

## Language and Technology Stack

### Primary Language: C++20

**This project is EXCLUSIVELY C++20. Do NOT use Python, Mojo, or other languages for implementation.**

#### Required Technologies:
- **C++20** with modules support
- **CMake** 3.20+ for build system
- **Google Test** (gtest) for unit and E2E testing
- **C++20 Coroutines** (`co_await`, `co_return`) for async operations
- **concurrentqueue** library for lock-free message queuing
- **ThreadPool** for parallel agent execution

#### Language Features Used:
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

### Agent Base Class

```cpp
class AgentBase {
public:
    virtual Task<void> processMessage(const KeystoneMessage& msg) = 0;
    virtual void run() = 0;
    virtual void stop() = 0;

protected:
    std::string agent_id_;
    MessageQueue inbox_;
    bool is_running_;
};
```

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
│   ├── agents/
│   ├── core/
│   └── concurrency/
├── tests/                   # Google Test files
│   ├── unit/                # Unit tests
│   └── e2e/                 # End-to-end tests
│       └── phase1/          # Phase 1 E2E tests
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
// E2E tests
TEST(E2E_Phase1, ChiefArchitectDelegatesToTaskAgent)
TEST(E2E_Phase2, ModuleLeadSynthesizesTaskResults)

// Unit tests
TEST(MessageQueue, PushAndPopMessage)
TEST(ThreadPool, SubmitAndExecuteTask)
```

### Git Workflow

- **Branch naming**: `claude/feature-name-<session-id>`
- **Commit format**: `feat:`, `fix:`, `docs:`, `test:`, `refactor:`
- **Always push** to feature branch before creating PR

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
TEST(E2E_Phase1, ChiefArchitectDelegatesToTaskAgent)
TEST(E2E_Phase1, ChiefArchitectCoordinatesThreeTaskAgents)
```

See [testing-strategy.md](docs/plan/testing-strategy.md) for complete test catalog.

### Unit Tests

Write unit tests for:
- MessageQueue operations
- ThreadPool functionality
- Message serialization/deserialization
- Coroutine helpers

### Test Execution

```bash
cd build
cmake ..
make
ctest --output-on-failure
```

## Build Commands

### Initial Setup
```bash
mkdir build && cd build
cmake ..
make
```

### Run Tests
```bash
ctest --output-on-failure
# OR
./tests/e2e/phase1_tests
```

### Clean Build
```bash
rm -rf build
mkdir build && cd build
cmake ..
make
```

## Dependencies

### Required Libraries

1. **Google Test** - Testing framework
   - Install: `apt-get install libgtest-dev` or build from source

2. **concurrentqueue** - Lock-free queue
   - Header-only library
   - Include in `third_party/concurrentqueue/`

3. **C++20 Compiler**
   - GCC 10+ or Clang 12+
   - Must support coroutines

### Dependency Management

- Use CMake `FetchContent` for external dependencies
- Or include as git submodules in `third_party/`

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
- ✅ All E2E tests passing

### Overall Success
- All 4 layers implemented
- E2E tests for all phases passing
- Performance benchmarks met (100+ agents)
- Chaos testing passes (20 random failures tolerated)

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
**Phase 1**: Implementing L0 (ChiefArchitect) ↔ L3 (TaskAgent)

### Current Test
E2E test: ChiefArchitect sends bash command to add two random numbers, TaskAgent executes and returns result

### Current Branch
`claude/first-agent-test-<session-id>`

---

**Last Updated**: 2025-11-17
**Version**: 1.0
**Project**: ProjectKeystone HMAS (C++20)
