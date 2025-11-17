# ProjectKeystone System Architecture

## Overview

ProjectKeystone implements a Hierarchical Multi-Agent System (HMAS) using modern C++20 features to achieve high-performance agent orchestration. The architecture is built on the Actor Model paradigm, ensuring isolation, asynchronous communication, and scalable concurrency.

## Architectural Principles

### 1. Actor-Model Foundation

Every agent in Keystone follows the Actor Model:
- **Isolated State**: Each agent maintains private internal state (shared-nothing architecture)
- **Message Passing**: All communication is asynchronous via dedicated mailboxes
- **Location Transparency**: Agents interact through well-defined message protocols
- **Supervision**: Parent agents supervise and manage child agent failures

### 2. Hierarchical Organization

The system is organized in three distinct layers:

```
                    ┌─────────────────┐
                    │   Root Agent    │ Layer 1 (L1)
                    │  (Orchestrator) │ Strategic Decisions
                    └────────┬────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
        ┌─────▼─────┐  ┌─────▼─────┐  ┌────▼──────┐
        │  Branch    │  │  Branch    │  │  Branch   │ Layer 2 (L2)
        │  Agent     │  │  Agent     │  │  Agent    │ Tactical Decisions
        └─────┬──────┘  └─────┬──────┘  └────┬──────┘
              │               │               │
          ┌───┼───┐       ┌───┼───┐      ┌───┼───┐
          │   │   │       │   │   │      │   │   │
        ┌─▼┐┌─▼┐┌─▼┐    ┌─▼┐┌─▼┐┌─▼┐   ┌─▼┐┌─▼┐┌─▼┐
        │L ││L ││L │    │L ││L ││L │   │L ││L ││L │ Layer 3 (L3)
        │e ││e ││e │    │e ││e ││e │   │e ││e ││e │ Execution
        │a ││a ││a │    │a ││a ││a │   │a ││a ││a │
        │f ││f ││f │    │f ││f ││f │   │f ││f ││f │
        └──┘└──┘└──┘    └──┘└──┘└──┘   └──┘└──┘└──┘
```

### 3. Separation of Concerns

Each layer has distinct responsibilities:

| Layer | Role | Responsibilities | Communication |
|-------|------|------------------|---------------|
| **L1: Root** | Orchestrator / Meta-Agent | Strategic planning, goal interpretation, initial task decomposition, global context management | Primarily vertical (downward delegation) |
| **L2: Branch** | Planning / Zone Manager | Tactical decomposition, result synthesis, transaction management, resource allocation | Vertical (up/down), limited horizontal |
| **L3: Leaf** | Worker / Tool Controller | Real-time execution, tool invocation, AI model inference | Vertical only (report upward) |

## Core Components

### 1. Agent Base Class

All agents inherit from `AgentBase`:

```cpp
module Keystone.Agents.Base;

import Keystone.Core.Messaging;
import Keystone.Protocol.KIM;
import <coroutine>;
import <string>;

export class AgentBase {
public:
    // Lifecycle
    virtual Task<void> initialize() = 0;
    virtual Task<void> run() = 0;  // Main coroutine loop
    virtual Task<void> shutdown() = 0;

    // Message handling
    Task<void> processMessage(const KeystoneMessage& msg);

    // State management
    AgentState getState() const;

protected:
    // Mailbox for incoming messages
    ConcurrentQueue<KeystoneMessage> mailbox_;

    // Agent state machine
    AgentState state_;

    // Unique agent identifier
    AgentId id_;

    // Session context
    SessionId session_id_;

    // Supervision
    AgentBase* supervisor_;
    std::vector<AgentBase*> supervised_agents_;
};
```

### 2. Message Bus Architecture

The message bus is the central nervous system of Keystone:

```
┌──────────────────────────────────────────────────────────┐
│                     Message Bus                          │
│                                                          │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐        │
│  │  Agent A   │  │  Agent B   │  │  Agent C   │        │
│  │            │  │            │  │            │        │
│  │ ┌────────┐ │  │ ┌────────┐ │  │ ┌────────┐ │        │
│  │ │Mailbox │◄┼──┼─┤Mailbox │◄┼──┼─┤Mailbox │ │        │
│  │ │ Queue  │ │  │ │ Queue  │ │  │ │ Queue  │ │        │
│  │ └────────┘ │  │ └────────┘ │  │ └────────┘ │        │
│  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘        │
│        │               │               │                │
│        └───────────────┴───────────────┘                │
│              concurrentqueue (lock-free)                │
└──────────────────────────────────────────────────────────┘
```

**Key Features**:
- Each agent has a dedicated `concurrentqueue<KeystoneMessage>` mailbox
- Lock-free, multi-producer, single-consumer design
- Custom `awaitable` for coroutine integration
- Active resumption: producers directly wake suspended consumers

### 3. Keystone Inter-Agent Message (KIM)

The standardized message format:

```cpp
module Keystone.Protocol.KIM;

export struct KeystoneMessage {
    // Routing
    UUID message_id;         // Unique message identifier
    AgentId sender_id;       // DID-like sender address
    AgentId recipient_id;    // DID-like recipient address

    // Control
    ActionType action_type;  // DECOMPOSE, EXECUTE_TOOL, RETURN_RESULT, etc.
    Priority priority;       // Message priority level

    // Payload
    std::string content_type;  // MIME type (e.g., "application/x-cista")
    std::vector<std::byte> payload;  // Serialized binary data

    // Context
    SessionId session_id;    // Session isolation identifier
    Timestamp timestamp;     // Message creation time
    Duration timeout;        // Maximum processing time

    // Metadata
    std::unordered_map<std::string, std::string> metadata;
};
```

### 4. Finite State Machine

Each agent implements a state machine for lifecycle management:

```
┌─────────┐
│  IDLE   │◄──────────────────┐
└────┬────┘                   │
     │ receive task           │
     ▼                        │
┌─────────┐                   │
│PLANNING │                   │
└────┬────┘                   │
     │ decompose              │
     ▼                        │
┌─────────────────┐           │
│WAITING_ON_CHILD │───────────┘ all children complete
└────┬────────────┘
     │ child failed
     ▼
┌─────────┐
│ERROR    │──┐ retry/recover
└─────────┘  │
     │       │
     │       ▼
     │  ┌─────────┐
     └──►EXECUTING│
        │  TOOL   │
        └────┬────┘
             │ complete
             ▼
        ┌─────────┐
        │SHUTTING │
        │  DOWN   │
        └─────────┘
```

## Concurrency Model

### 1. C++20 Coroutines

Agents use coroutines for asynchronous execution:

```cpp
// Agent main loop
Task<void> AgentBase::run() {
    state_ = AgentState::IDLE;

    while (state_ != AgentState::SHUTTING_DOWN) {
        // Asynchronously wait for message
        auto msg = co_await AsyncQueuePop{mailbox_};

        // Process message based on state
        co_await processMessage(msg);
    }

    co_await shutdown();
}
```

### 2. Custom Awaitable for Queue Operations

The `AsyncQueuePop` awaitable integrates the queue with coroutines:

```cpp
template<typename T>
class AsyncQueuePop {
public:
    explicit AsyncQueuePop(ConcurrentQueue<T>& queue)
        : queue_(queue) {}

    // Check if message is immediately available
    bool await_ready() {
        return queue_.try_pop(value_);
    }

    // Suspend and register for notification
    void await_suspend(std::coroutine_handle<> handle) {
        queue_.registerWaiter(handle);
    }

    // Return the popped value
    T await_resume() {
        return std::move(value_);
    }

private:
    ConcurrentQueue<T>& queue_;
    T value_;
};
```

### 3. Thread Pool

Fixed-size thread pool for coroutine execution:

```cpp
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());

    // Submit work (function or coroutine handle)
    template<typename F>
    void submit(F&& work);

    // Graceful shutdown
    void shutdown();

private:
    std::vector<std::thread> workers_;
    ConcurrentQueue<std::function<void()>> work_queue_;
    std::atomic<bool> shutdown_requested_{false};
};
```

### 4. Synchronization Primitives

**std::latch** for one-time initialization:
```cpp
// Wait for all core components to initialize
std::latch initialization_complete{3};  // Message bus, thread pool, root agent

// In each component:
component.initialize();
initialization_complete.count_down();

// In main:
initialization_complete.wait();  // All components ready
```

**std::barrier** for recurring phase synchronization:
```cpp
// Branch agent waiting for all leaf children
std::barrier phase_completion{num_children};

// Each leaf agent:
complete_execution();
phase_completion.arrive_and_wait();  // Synchronize

// Branch agent continues to synthesis phase
```

## Communication Patterns

### 1. Vertical Communication (Primary)

**Downward Delegation**:
```
Root Agent
    ├─> DECOMPOSE task → Branch Agent 1
    ├─> DECOMPOSE task → Branch Agent 2
    └─> DECOMPOSE task → Branch Agent 3
```

**Upward Aggregation**:
```
Leaf Agents
    ├─> RETURN_RESULT → Branch Agent
    └─> RETURN_RESULT → Branch Agent
            └─> SYNTHESIZE → Root Agent
```

### 2. Horizontal Communication (Limited)

Only Branch agents can communicate horizontally for:
- Load balancing
- Resource negotiation
- Conflict resolution

### 3. External Communication

Leaf agents act as gateways to external systems:

```
Leaf Agent
    ├─> gRPC → Remote AI Service (OpenAI, Anthropic, etc.)
    ├─> ONNX Runtime → Local Model
    └─> HTTP/REST → External APIs
```

## Serialization Strategy

### Internal (High-Performance)

Uses **Cista** for zero-copy serialization:

```cpp
#include <cista.h>

struct TaskContext {
    cista::offset::string goal;
    cista::offset::vector<cista::offset::string> sub_tasks;
    int64_t timestamp;
};

// Serialize (minimal overhead)
auto serialized = cista::serialize<TaskContext>(context);

// Deserialize (zero-copy, ~0.002ms)
auto* deserialized = cista::deserialize<TaskContext>(serialized);
```

### External (Interoperable)

Uses **Protocol Buffers** for gRPC:

```protobuf
syntax = "proto3";

service AIModelService {
    rpc GenerateText(TextRequest) returns (TextResponse);
}

message TextRequest {
    string prompt = 1;
    int32 max_tokens = 2;
    float temperature = 3;
}
```

### Serialization Gateway (Leaf Agent)

```cpp
Task<void> LeafAgent::executeRemoteInference(const KeystoneMessage& msg) {
    // 1. Deserialize internal Cista payload
    auto* task = cista::deserialize<InferenceTask>(msg.payload);

    // 2. Transform to Protobuf request
    TextRequest proto_request;
    proto_request.set_prompt(task->prompt.str());
    proto_request.set_max_tokens(task->max_tokens);

    // 3. Execute gRPC call
    auto response = co_await grpc_client_.GenerateText(proto_request);

    // 4. Transform back to Cista for internal use
    InferenceResult result;
    result.text = cista::offset::string{response.text()};

    // 5. Send result up the hierarchy
    co_await sendResult(cista::serialize(result));
}
```

## Supervision and Failure Recovery

### Supervision Tree

```
Root Agent (L1)
├── supervises → Branch Agent 1 (L2)
│   ├── supervises → Leaf Agent 1a (L3)
│   ├── supervises → Leaf Agent 1b (L3)
│   └── supervises → Leaf Agent 1c (L3)
├── supervises → Branch Agent 2 (L2)
│   └── supervises → Leaf Agents...
└── supervises → Branch Agent 3 (L2)
    └── supervises → Leaf Agents...
```

### Failure Handling Strategies

**Leaf Agent Failure**:
1. Branch agent detects timeout or error state
2. Branch agent attempts:
   - Restart failed leaf agent (if transient error)
   - Delegate to peer leaf agent (horizontal failover)
   - Mark sub-task as failed and report up
3. Transaction state preserved for retry

**Branch Agent Failure**:
1. Root agent detects failure
2. Root agent options:
   - Restart branch with preserved state
   - Re-decompose and delegate to different branch
   - Abort workflow and report failure to user

**Root Agent Failure**:
- Critical failure requiring system restart
- External supervisor (e.g., Kubernetes) handles recovery

## Graceful Shutdown Protocol

```
1. Signal Received (SIGTERM/SIGINT)
   └─> Set global ShutdownRequested flag

2. Root Agent
   ├─> Stop accepting new tasks
   └─> Send SHUTDOWN to all Branch agents

3. Branch Agents (parallel)
   ├─> Stop accepting new tasks
   ├─> Send SHUTDOWN to all Leaf agents
   └─> Wait for Leaf agents to drain

4. Leaf Agents (parallel)
   ├─> Complete current task
   ├─> Drain mailbox
   └─> Report completion to Branch

5. Branch Agents
   ├─> Synthesize final results
   ├─> Drain mailbox
   └─> Report completion to Root

6. Root Agent
   ├─> Finalize any pending results
   ├─> Drain mailbox
   └─> Signal shutdown complete

7. Thread Pool
   └─> Join all worker threads

8. Clean Exit
```

## Performance Optimizations

### 1. Lock-Free Data Structures
- `concurrentqueue` for mailboxes
- Atomic operations for state transitions
- Memory ordering guarantees

### 2. Zero-Copy Serialization
- Cista for internal messages
- Direct memory mapping
- Minimal allocation overhead

### 3. Active Resumption
- Producers directly wake suspended consumers
- No polling loops
- Sub-millisecond notification latency

### 4. Thread Pool Sizing
- Default: `std::thread::hardware_concurrency()`
- Configurable based on workload
- Work-stealing for load balancing

### 5. Memory Pools
- Pre-allocated message buffers
- Object pools for frequent allocations
- NUMA-aware allocation strategies

## Security Considerations

### Session Isolation
- Unique `SessionId` per request
- All resources scoped to session
- Prevents cross-session data leakage

### Input Validation
- Message structure validation
- Payload size limits
- Timeout enforcement

### Resource Limits
- Maximum mailbox depth per agent
- Configurable memory limits
- CPU time quotas

## Monitoring and Observability

### Metrics Collection
- Mailbox depth per agent
- Message processing latency
- State transition frequency
- Error rates by agent type

### Distributed Tracing
- Message correlation via `message_id`
- Session tracking via `session_id`
- End-to-end request tracing

### Health Checks
- Agent liveness probes
- Mailbox congestion detection
- Supervision tree health

---

**Document Version**: 1.0
**Last Updated**: 2025-11-17
