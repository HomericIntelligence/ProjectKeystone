# Two-Agent Architecture (Phase 1)

## Overview

The initial implementation uses a simplified **two-agent model** to validate core infrastructure before scaling to the full three-layer hierarchy. This approach follows TDD principles: build the simplest thing that works, validate end-to-end, then expand.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                        User/Client                          │
└───────────────────────┬─────────────────────────────────────┘
                        │
                        │ processGoal("Calculate: 2+2")
                        ▼
┌─────────────────────────────────────────────────────────────┐
│                   CoordinatorAgent                          │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Responsibilities:                                     │  │
│  │ • Receive user goals                                 │  │
│  │ • Decompose into executable tasks                    │  │
│  │ • Route tasks to Worker via MessageBus               │  │
│  │ • Aggregate results from Worker                      │  │
│  │ • Return final response to user                      │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
│  State Machine:                                             │
│  IDLE → PLANNING → WAITING_FOR_WORKER → AGGREGATING → IDLE │
│                                                             │
│  Mailbox: ConcurrentQueue<KeystoneMessage>                 │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       │ KeystoneMessage{
                       │   action: EXECUTE_TASK,
                       │   payload: "add(2, 2)"
                       │ }
                       ▼
          ┌────────────────────────────┐
          │      MessageBus            │
          │  (Routes by recipient_id)  │
          └────────────┬───────────────┘
                       │
                       │ Delivers to Worker's mailbox
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                      WorkerAgent                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Responsibilities:                                     │  │
│  │ • Receive tasks from Coordinator                     │  │
│  │ • Execute computation/tool invocation                │  │
│  │ • Call external services (gRPC AI models)            │  │
│  │ • Send results back to Coordinator                   │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
│  State Machine:                                             │
│  IDLE → EXECUTING_TASK → IDLE                              │
│                                                             │
│  Mailbox: ConcurrentQueue<KeystoneMessage>                 │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       │ KeystoneMessage{
                       │   action: RETURN_RESULT,
                       │   payload: "4"
                       │ }
                       ▼
          ┌────────────────────────────┐
          │      MessageBus            │
          └────────────┬───────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                   CoordinatorAgent                          │
│  Receives result, aggregates, returns to user               │
└─────────────────────────────────────────────────────────────┘
                       │
                       │ returns "Result: 4"
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                        User/Client                          │
└─────────────────────────────────────────────────────────────┘
```

## Component Details

### CoordinatorAgent

**Purpose**: High-level orchestration and user interface

**Key Methods**:
```cpp
class CoordinatorAgent : public AgentBase {
public:
    // Lifecycle
    Task<void> initialize() override;
    Task<void> run() override;          // Main coroutine loop
    Task<void> shutdown() override;

    // User interface
    std::future<std::string> processGoal(const std::string& goal);

    // Orchestration
    Task<void> decomposeGoal(const std::string& goal);
    Task<void> delegateTask(const Task& task);
    Task<std::string> aggregateResults();

private:
    // Worker registry
    std::vector<AgentId> workers_;

    // Pending results
    std::unordered_map<UUID, std::promise<std::string>> pending_goals_;
};
```

**State Machine**:
```
IDLE
  │ processGoal() called
  ▼
PLANNING
  │ decomposeGoal()
  ▼
WAITING_FOR_WORKER
  │ co_await receiveMessage()
  ▼
AGGREGATING
  │ aggregateResults()
  ▼
IDLE (return result to user)
```

**Message Flow**:
1. Receives goal from user (API call)
2. Creates `KeystoneMessage` with `EXECUTE_TASK` action
3. Sends to Worker via `MessageBus`
4. Suspends coroutine (`co_await`) waiting for result
5. Receives result message from Worker
6. Aggregates and returns to user

### WorkerAgent

**Purpose**: Task execution and external service integration

**Key Methods**:
```cpp
class WorkerAgent : public AgentBase {
public:
    // Lifecycle
    Task<void> initialize() override;
    Task<void> run() override;
    Task<void> shutdown() override;

    // Execution
    Task<void> executeTask(const KeystoneMessage& task_msg);
    Task<std::string> invokeToolchain(const std::string& task_spec);

    // External integration (Phase 4)
    void setAIServiceEndpoint(const std::string& endpoint);
    Task<std::string> callAIService(const std::string& prompt);

private:
    // Tool registry
    std::unordered_map<std::string, ToolFunction> tools_;

    // gRPC client (Phase 4)
    std::unique_ptr<GRPCClient> ai_client_;
};
```

**State Machine**:
```
IDLE
  │ receiveMessage(EXECUTE_TASK)
  ▼
EXECUTING_TASK
  │ executeTask()
  │ (may call external AI service)
  ▼
IDLE (send result back to Coordinator)
```

**Message Flow**:
1. Receives `EXECUTE_TASK` message from Coordinator
2. Executes task (computation, tool invocation, AI call)
3. Creates result `KeystoneMessage` with `RETURN_RESULT` action
4. Sends result to Coordinator via `MessageBus`

### MessageBus

**Purpose**: Central routing of messages between agents

**Key Features**:
- Agent registration by `AgentId`
- Message routing by `recipient_id`
- Thread-safe delivery to agent mailboxes
- Optional message tracing for debugging

**Implementation**:
```cpp
class MessageBus {
public:
    // Registration
    void registerAgent(AgentId id, AgentBase* agent);
    void unregisterAgent(AgentId id);

    // Routing
    void send(const KeystoneMessage& msg);
    bool send_with_timeout(const KeystoneMessage& msg, Duration timeout);

    // Introspection
    size_t getAgentCount() const;
    std::vector<AgentId> getRegisteredAgents() const;

private:
    // Agent registry
    std::unordered_map<AgentId, AgentBase*> agents_;
    std::shared_mutex registry_mutex_;
};
```

## Message Protocol (Simplified)

### KeystoneMessage Structure

```cpp
struct KeystoneMessage {
    // Routing
    UUID message_id;          // Unique message ID
    AgentId sender_id;        // "coordinator_1"
    AgentId recipient_id;     // "worker_1"

    // Action
    ActionType action;        // EXECUTE_TASK, RETURN_RESULT

    // Payload (Cista serialized)
    std::vector<std::byte> payload;

    // Metadata
    SessionId session_id;     // For context isolation
    Timestamp timestamp;
};
```

### Action Types (Phase 1)

```cpp
enum class ActionType {
    EXECUTE_TASK,      // Coordinator → Worker: Execute this task
    RETURN_RESULT,     // Worker → Coordinator: Here's the result
    HEALTH_CHECK,      // Ping/pong for liveness
    SHUTDOWN           // Graceful shutdown signal
};
```

### Example Message Flow

**Step 1: Coordinator sends task to Worker**
```cpp
KeystoneMessage task_msg{
    .message_id = UUID::generate(),
    .sender_id = "coordinator_1",
    .recipient_id = "worker_1",
    .action = ActionType::EXECUTE_TASK,
    .payload = serialize(TaskSpec{
        .operation = "add",
        .arguments = {"2", "2"}
    }),
    .session_id = "session_abc123",
    .timestamp = now()
};

message_bus.send(task_msg);
```

**Step 2: Worker executes and sends result**
```cpp
// Worker executes task
auto result = execute_add("2", "2");  // → "4"

KeystoneMessage result_msg{
    .message_id = UUID::generate(),
    .sender_id = "worker_1",
    .recipient_id = "coordinator_1",
    .action = ActionType::RETURN_RESULT,
    .payload = serialize(TaskResult{
        .original_task_id = task_msg.message_id,
        .result = "4",
        .status = "success"
    }),
    .session_id = "session_abc123",
    .timestamp = now()
};

message_bus.send(result_msg);
```

## Concurrency Model

### Thread Pool

```
┌──────────────────────────────────────────────┐
│            ThreadPool (4 threads)            │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐       │
│  │Thread 1 │ │Thread 2 │ │Thread 3 │  ...  │
│  └────┬────┘ └────┬────┘ └────┬────┘       │
│       │           │           │             │
│       └───────────┴───────────┘             │
│                   │                         │
│       Shared Work Queue (coroutine handles) │
└───────────────────┬─────────────────────────┘
                    │
          ┌─────────┼─────────┐
          │         │         │
          ▼         ▼         ▼
    ┌─────────┐ ┌─────────┐ ┌─────────┐
    │Coord    │ │Worker   │ │Worker   │
    │Coroutine│ │Coroutine│ │Coroutine│
    └─────────┘ └─────────┘ └─────────┘
```

### Agent Run Loop (Coroutine)

```cpp
Task<void> CoordinatorAgent::run() {
    while (state_ != AgentState::SHUTTING_DOWN) {
        // Asynchronously wait for message (suspends coroutine)
        auto msg = co_await AsyncQueuePop{mailbox_};

        // Process based on action type
        switch (msg.action) {
            case ActionType::EXECUTE_TASK:
                // Shouldn't receive tasks (we're the coordinator)
                logError("Received unexpected EXECUTE_TASK");
                break;

            case ActionType::RETURN_RESULT:
                // Worker sent result
                co_await handleWorkerResult(msg);
                break;

            case ActionType::SHUTDOWN:
                state_ = AgentState::SHUTTING_DOWN;
                break;
        }
    }

    co_await shutdown();
}
```

### Custom Awaitable (AsyncQueuePop)

```cpp
template<typename T>
class AsyncQueuePop {
public:
    explicit AsyncQueuePop(MessageQueue<T>& queue) : queue_(queue) {}

    // Check if message immediately available
    bool await_ready() {
        return queue_.try_pop(value_);
    }

    // Suspend and register for notification
    void await_suspend(std::coroutine_handle<> handle) {
        // Register this coroutine to be resumed when message arrives
        queue_.registerWaiter(handle);
    }

    // Return the message
    T await_resume() {
        return std::move(value_);
    }

private:
    MessageQueue<T>& queue_;
    T value_;
};
```

## Scaling to Three Layers

### Evolution Path

**Phase 1 (Current):**
```
Coordinator → Worker
```

**Phase 2 (Multiple Workers):**
```
Coordinator → Worker 1
           ├→ Worker 2
           └→ Worker 3
```

**Phase 3 (Add Branch Layer):**
```
Root → Branch 1 → Leaf 1
    │           ├→ Leaf 2
    │           └→ Leaf 3
    ├→ Branch 2 → Leaves...
    └→ Branch 3 → Leaves...
```

**Key Insight**: The two-agent model validates ALL core infrastructure:
- Message passing ✅
- Coroutines ✅
- State machines ✅
- Serialization ✅
- External calls ✅

Adding the Branch layer is just:
1. Rename `CoordinatorAgent` → `RootAgent`
2. Rename `WorkerAgent` → `LeafAgent`
3. Create `BranchAgent` (combines both patterns: receives from Root, delegates to Leaves)

## Performance Characteristics

### Latency Breakdown (Target)

```
User request → Coordinator: ~10μs (local call)
Coordinator decomposition: ~50μs
Coordinator → MessageBus → Worker: ~500μs (queue operations)
Worker execution: ~100μs (simple task) to ~10ms (AI call)
Worker → MessageBus → Coordinator: ~500μs
Coordinator aggregation: ~50μs
Total: ~1.2ms (simple) to ~11.2ms (with AI)
```

### Throughput (Target)

```
Single Worker: ~1,000 tasks/second
3 Workers: ~3,000 tasks/second
10 Workers: ~10,000 tasks/second
```

## Testing Strategy

### E2E Test Coverage

1. **Basic Delegation** (Week 3):
   - Coordinator sends task, Worker returns result
   - Verify message flow
   - Verify state transitions

2. **Multiple Workers** (Week 4):
   - Coordinator load-balances across 3 workers
   - Verify concurrent execution
   - Verify result aggregation

3. **Error Handling** (Week 5):
   - Worker failure (timeout)
   - Worker exception
   - Malformed messages

4. **Performance** (Week 6):
   - 1000 tasks/second throughput
   - <10ms latency (p99)
   - ThreadSanitizer validation

5. **External Integration** (Week 8):
   - Worker calls mock gRPC service
   - Timeout handling
   - Retry logic

## Advantages of Two-Agent Start

✅ **Simplicity**: Minimal complexity, easy to reason about
✅ **Fast validation**: Core infrastructure proven quickly
✅ **Incremental**: Add Branch layer only when needed
✅ **Debugging**: Easier to trace with only 2 agents
✅ **Performance baseline**: Establish metrics before scaling
✅ **TDD-friendly**: Simple E2E tests, fast iteration

---

**Document Version**: 1.0
**Last Updated**: 2025-11-17
**Related**: TDD_APPROACH.md, architecture.md
