# Phase 10: Advanced Features Plan

**Status**: 📝 Planning
**Date Started**: TBD
**Target Completion**: TBD
**Branch**: TBD

## Overview

Phase 10 adds **advanced production features** to the ProjectKeystone HMAS including message persistence, distributed tracing, A/B testing framework, and auto-scaling capabilities. These features transform the system into a fully-featured, enterprise-grade distributed agent system.

### Current Status (Post-Phase 5)

**What We Have**:
- ✅ Full 4-layer async hierarchy
- ✅ Multi-component coordination
- ✅ Chaos engineering infrastructure
- ✅ Metrics and monitoring
- ✅ 329/329 tests passing

**What Phase 10 Adds**:
- Message persistence and replay (RocksDB/LevelDB)
- Distributed tracing (OpenTelemetry + Jaeger)
- A/B testing framework for agent strategies
- Auto-scaling based on load
- Advanced debugging and observability

---

## Phase 10 Architecture

```
┌─────────────────────────────────────────────────────┐
│ Message Persistence Layer                           │
│  ┌────────────────────────────────────────────┐    │
│  │ RocksDB/LevelDB                            │    │
│  │  - Durable message queues                  │    │
│  │  - WAL for crash recovery                  │    │
│  │  - Message replay capability               │    │
│  └────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ Distributed Tracing Layer                           │
│  ┌────────────────────────────────────────────┐    │
│  │ OpenTelemetry                              │    │
│  │  - Trace context propagation               │    │
│  │  - Span creation per message hop           │    │
│  │  - Trace export to Jaeger/Zipkin           │    │
│  └────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ A/B Testing Framework                               │
│  ┌────────────────────────────────────────────┐    │
│  │ Strategy Variants                          │    │
│  │  - Variant A: Baseline strategy            │    │
│  │  - Variant B: Experimental strategy        │    │
│  │  - Traffic splitting (50/50, 90/10)        │    │
│  │  - Metrics comparison and rollback         │    │
│  └────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ Auto-Scaling Controller                             │
│  ┌────────────────────────────────────────────┐    │
│  │ Load Monitoring                            │    │
│  │  - Queue depth monitoring                  │    │
│  │  - CPU/memory utilization                  │    │
│  │  - Message throughput                      │    │
│  └────────────────────────────────────────────┘    │
│  ┌────────────────────────────────────────────┐    │
│  │ Agent Spawning/Termination                 │    │
│  │  - Dynamic TaskAgent creation              │    │
│  │  - Graceful agent shutdown                 │    │
│  │  - Resource allocation management          │    │
│  └────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────┘
```

---

## Phase 10 Implementation Plan

### Phase 10.1: Message Persistence (Week 1-2)

**Goal**: Add durable message storage with crash recovery

**Tasks**:

1. **RocksDB Integration** (12 hours)
   ```cpp
   // include/persistence/message_store.hpp
   class MessageStore {
   public:
       MessageStore(const std::string& db_path);
       ~MessageStore();

       Task<void> append(const KeystoneMessage& msg);
       Task<std::optional<KeystoneMessage>> get(const std::string& msg_id);
       Task<std::vector<KeystoneMessage>> getRange(int64_t start_ts, int64_t end_ts);
       Task<void> delete_(const std::string& msg_id);

       // Replay support
       Task<std::vector<KeystoneMessage>> getAllUnprocessed();
       Task<void> markProcessed(const std::string& msg_id);

   private:
       std::unique_ptr<rocksdb::DB> db_;
       std::string db_path_;

       std::string serializeMessage(const KeystoneMessage& msg);
       KeystoneMessage deserializeMessage(const std::string& data);
   };
   ```

2. **Write-Ahead Log (WAL)** (10 hours)
   ```cpp
   // include/persistence/wal.hpp
   class WriteAheadLog {
   public:
       WriteAheadLog(const std::string& wal_path);

       Task<void> log(const KeystoneMessage& msg);
       Task<std::vector<KeystoneMessage>> replay();
       Task<void> truncate(int64_t before_timestamp);

   private:
       std::ofstream wal_file_;
       std::string wal_path_;
   };
   ```

3. **Persistent MessageQueue** (12 hours)
   ```cpp
   // include/persistence/persistent_message_queue.hpp
   class PersistentMessageQueue : public MessageQueue {
   public:
       PersistentMessageQueue(std::unique_ptr<MessageStore> store,
                             std::unique_ptr<WriteAheadLog> wal);

       Task<void> push(const KeystoneMessage& msg) override;
       Task<std::optional<KeystoneMessage>> pop() override;

       Task<void> recoverFromCrash();

   private:
       std::unique_ptr<MessageStore> store_;
       std::unique_ptr<WriteAheadLog> wal_;
   };
   ```

4. **Crash Recovery** (10 hours)
   - On startup, replay WAL
   - Requeue unprocessed messages
   - Delete processed messages
   - Verify message ordering

5. **E2E Test: Crash Recovery** (8 hours)
   - Send 100 messages
   - Simulate crash after 50 processed
   - Restart system
   - Verify remaining 50 messages processed

**Deliverables**:
- ✅ RocksDB integration
- ✅ MessageStore class
- ✅ Write-Ahead Log
- ✅ PersistentMessageQueue
- ✅ Crash recovery mechanism
- ✅ E2E test: Crash recovery

**Estimated Time**: 52 hours

---

### Phase 10.2: Distributed Tracing (Week 3-4)

**Goal**: Trace message flow across the entire hierarchy

**Tasks**:

1. **OpenTelemetry Integration** (16 hours)
   ```cpp
   // include/tracing/tracer.hpp
   class Tracer {
   public:
       Tracer(const std::string& service_name);

       struct Span {
           std::string trace_id;
           std::string span_id;
           std::string parent_span_id;
           std::string operation_name;
           std::chrono::system_clock::time_point start_time;
           std::chrono::system_clock::time_point end_time;
           std::map<std::string, std::string> tags;
       };

       Span startSpan(const std::string& operation_name,
                      const std::optional<std::string>& parent_span_id = std::nullopt);
       void finishSpan(Span& span);
       void addTag(Span& span, const std::string& key, const std::string& value);

   private:
       std::unique_ptr<opentelemetry::sdk::trace::TracerProvider> provider_;
       std::shared_ptr<opentelemetry::trace::Tracer> tracer_;
   };
   ```

2. **Trace Context Propagation** (12 hours)
   ```cpp
   // Enhance KeystoneMessage with trace context
   struct KeystoneMessage {
       // ... existing fields ...

       std::string trace_id;
       std::string span_id;
       std::string parent_span_id;
   };

   // When sending message
   void AsyncBaseAgent::sendMessage(const KeystoneMessage& msg) {
       auto span = tracer_->startSpan("sendMessage");

       KeystoneMessage msg_with_trace = msg;
       msg_with_trace.trace_id = span.trace_id;
       msg_with_trace.span_id = span.span_id;

       // Send message
       message_bus_->routeMessage(msg_with_trace);

       tracer_->finishSpan(span);
   }
   ```

3. **Jaeger Exporter** (10 hours)
   ```cpp
   // include/tracing/jaeger_exporter.hpp
   class JaegerExporter {
   public:
       JaegerExporter(const std::string& jaeger_endpoint);

       void exportSpan(const Tracer::Span& span);

   private:
       std::string jaeger_endpoint_;
       std::unique_ptr<httplib::Client> http_client_;
   };
   ```

4. **Automatic Span Creation** (12 hours)
   - Create span on message send
   - Create child span on message receive
   - Create span for task execution
   - Link spans across agent hierarchy

5. **E2E Test: Trace Visualization** (8 hours)
   - Send message from Chief to TaskAgent
   - Verify trace spans created (4 spans: Chief → Component → Module → Task)
   - Export to Jaeger
   - Visualize trace in Jaeger UI

**Deliverables**:
- ✅ OpenTelemetry integration
- ✅ Tracer class
- ✅ Trace context propagation
- ✅ Jaeger exporter
- ✅ Automatic span creation
- ✅ E2E test: Trace visualization

**Estimated Time**: 58 hours

---

### Phase 10.3: A/B Testing Framework (Week 5)

**Goal**: Test different agent strategies and compare metrics

**Tasks**:

1. **Strategy Interface** (8 hours)
   ```cpp
   // include/ab_testing/agent_strategy.hpp
   class AgentStrategy {
   public:
       virtual ~AgentStrategy() = default;

       virtual Task<Response> processMessage(const KeystoneMessage& msg) = 0;
       virtual std::string getStrategyName() const = 0;
   };

   // Example strategies
   class BaselineStrategy : public AgentStrategy {
       Task<Response> processMessage(const KeystoneMessage& msg) override;
       std::string getStrategyName() const override { return "baseline"; }
   };

   class ExperimentalStrategy : public AgentStrategy {
       Task<Response> processMessage(const KeystoneMessage& msg) override;
       std::string getStrategyName() const override { return "experimental"; }
   };
   ```

2. **Traffic Splitter** (10 hours)
   ```cpp
   // include/ab_testing/traffic_splitter.hpp
   class TrafficSplitter {
   public:
       struct VariantConfig {
           std::string variant_name;
           std::unique_ptr<AgentStrategy> strategy;
           double traffic_percentage;  // 0.0 to 1.0
       };

       TrafficSplitter(std::vector<VariantConfig> variants);

       AgentStrategy* selectStrategy(const KeystoneMessage& msg);

   private:
       std::vector<VariantConfig> variants_;
       std::mt19937 rng_;
   };
   ```

3. **Metrics Collector** (10 hours)
   ```cpp
   // include/ab_testing/variant_metrics.hpp
   class VariantMetrics {
   public:
       void recordLatency(const std::string& variant, double latency_ms);
       void recordSuccess(const std::string& variant);
       void recordFailure(const std::string& variant);

       struct VariantStats {
           double mean_latency_ms;
           double p95_latency_ms;
           double p99_latency_ms;
           double success_rate;
           int total_requests;
       };

       VariantStats getStats(const std::string& variant) const;
       bool isSignificantlyBetter(const std::string& variant_a,
                                 const std::string& variant_b,
                                 double confidence_level = 0.95) const;

   private:
       std::unordered_map<std::string, std::vector<double>> latencies_;
       std::unordered_map<std::string, int> successes_;
       std::unordered_map<std::string, int> failures_;
   };
   ```

4. **Automated Rollback** (8 hours)
   ```cpp
   // include/ab_testing/rollback_controller.hpp
   class RollbackController {
   public:
       RollbackController(std::unique_ptr<VariantMetrics> metrics);

       void startExperiment(const std::string& baseline_variant,
                           const std::string& experimental_variant);

       bool shouldRollback();  // Check if experimental is worse
       void rollback();        // Switch all traffic to baseline

   private:
       std::unique_ptr<VariantMetrics> metrics_;
       std::string baseline_variant_;
       std::string experimental_variant_;
   };
   ```

5. **E2E Test: A/B Experiment** (8 hours)
   - Create 2 strategies (baseline: fast but inaccurate, experimental: slow but accurate)
   - Split traffic 50/50
   - Process 1000 messages
   - Compare metrics
   - Verify experimental strategy selected if better

**Deliverables**:
- ✅ AgentStrategy interface
- ✅ TrafficSplitter class
- ✅ VariantMetrics class
- ✅ Automated rollback controller
- ✅ E2E test: A/B experiment

**Estimated Time**: 44 hours

---

### Phase 10.4: Auto-Scaling (Week 6-7)

**Goal**: Dynamically scale TaskAgent count based on load

**Tasks**:

1. **Load Metrics Collector** (10 hours)
   ```cpp
   // include/autoscaling/load_metrics.hpp
   class LoadMetrics {
   public:
       void recordQueueDepth(int depth);
       void recordCpuUsage(double percentage);
       void recordMemoryUsage(double percentage);
       void recordThroughput(int messages_per_second);

       struct LoadStats {
           double avg_queue_depth;
           double max_queue_depth;
           double cpu_usage;
           double memory_usage;
           double throughput;
       };

       LoadStats getStats() const;
       bool isOverloaded() const;
       bool isUnderloaded() const;

   private:
       std::vector<int> queue_depths_;
       std::vector<double> cpu_samples_;
       std::vector<double> memory_samples_;
       std::vector<int> throughput_samples_;
   };
   ```

2. **Auto-Scaling Controller** (16 hours)
   ```cpp
   // include/autoscaling/autoscaler.hpp
   class AutoScaler {
   public:
       struct ScalingPolicy {
           int min_agents = 1;
           int max_agents = 100;
           double scale_up_threshold = 0.8;    // 80% queue full
           double scale_down_threshold = 0.2;  // 20% queue full
           int cooldown_seconds = 60;
       };

       AutoScaler(ScalingPolicy policy,
                 std::unique_ptr<LoadMetrics> load_metrics);

       void start();
       void stop();

       Task<void> scaleUp();
       Task<void> scaleDown();

   private:
       ScalingPolicy policy_;
       std::unique_ptr<LoadMetrics> load_metrics_;
       std::vector<std::unique_ptr<AsyncTaskAgent>> agents_;
       std::atomic<bool> running_{false};
       std::thread scaling_thread_;

       void scalingLoop();
       bool shouldScaleUp();
       bool shouldScaleDown();
   };
   ```

3. **Agent Spawning** (12 hours)
   ```cpp
   // Enhance AsyncTaskAgent for dynamic creation
   class AsyncTaskAgent {
   public:
       static std::unique_ptr<AsyncTaskAgent> spawn(const std::string& agent_id,
                                                    MessageBus* bus,
                                                    WorkStealingScheduler* scheduler);

       Task<void> shutdown();

   private:
       void registerWithMessageBus();
       void unregisterFromMessageBus();
   };
   ```

4. **Graceful Agent Shutdown** (10 hours)
   - Drain message queue before shutdown
   - Complete in-progress tasks
   - Notify MessageBus of shutdown
   - Release resources

5. **E2E Test: Auto-Scaling** (12 hours)
   - Start with 2 TaskAgents
   - Send 1000 messages (queue overload)
   - Verify AutoScaler spawns new agents (up to 10)
   - Wait for queue to drain
   - Verify AutoScaler terminates idle agents (down to 2)

**Deliverables**:
- ✅ LoadMetrics class
- ✅ AutoScaler controller
- ✅ Dynamic agent spawning
- ✅ Graceful agent shutdown
- ✅ E2E test: Auto-scaling

**Estimated Time**: 60 hours

---

### Phase 10.5: Advanced Debugging (Week 8)

**Goal**: Provide advanced debugging and observability tools

**Tasks**:

1. **Message Replay Tool** (10 hours)
   ```cpp
   // include/debugging/message_replay.hpp
   class MessageReplay {
   public:
       MessageReplay(std::unique_ptr<MessageStore> store);

       Task<void> replayRange(int64_t start_ts, int64_t end_ts);
       Task<void> replayMessage(const std::string& msg_id);

   private:
       std::unique_ptr<MessageStore> store_;
   };
   ```

2. **Agent State Inspector** (8 hours)
   ```cpp
   // include/debugging/agent_inspector.hpp
   class AgentInspector {
   public:
       struct AgentState {
           std::string agent_id;
           std::string state;  // "IDLE", "PROCESSING", "FAILED"
           int queue_depth;
           int tasks_completed;
           int tasks_failed;
           std::vector<std::string> recent_messages;
       };

       AgentState inspectAgent(const std::string& agent_id);
       std::vector<AgentState> inspectAllAgents();
   };
   ```

3. **Performance Profiler** (12 hours)
   ```cpp
   // include/debugging/profiler.hpp
   class Profiler {
   public:
       void startProfiling(const std::string& profile_name);
       void stopProfiling(const std::string& profile_name);

       struct ProfilingResult {
           std::string name;
           double total_time_ms;
           int call_count;
           double avg_time_ms;
           double max_time_ms;
           double min_time_ms;
       };

       std::vector<ProfilingResult> getResults() const;
       void exportFlameGraph(const std::string& output_path);
   };
   ```

4. **Debug CLI** (10 hours)
   ```bash
   # Command-line interface for debugging
   $ hmas-debug inspect agent task_1
   Agent ID: task_1
   State: PROCESSING
   Queue Depth: 5
   Tasks Completed: 42
   Tasks Failed: 1

   $ hmas-debug replay message msg_12345
   Replaying message msg_12345...
   Result: SUCCESS

   $ hmas-debug profile
   Top 10 Functions by Time:
   1. processMessage: 450ms (150 calls, avg 3ms)
   2. routeMessage: 120ms (500 calls, avg 0.24ms)
   ```

**Deliverables**:
- ✅ MessageReplay tool
- ✅ AgentInspector class
- ✅ Performance profiler
- ✅ Debug CLI

**Estimated Time**: 40 hours

---

## Success Criteria

### Must Have ✅
- [ ] Message persistence (RocksDB)
- [ ] Crash recovery working
- [ ] Distributed tracing (OpenTelemetry)
- [ ] Trace export to Jaeger
- [ ] A/B testing framework functional
- [ ] Auto-scaling based on load
- [ ] Message replay tool

### Should Have 🎯
- [ ] Write-Ahead Log (WAL)
- [ ] Automatic span creation across hierarchy
- [ ] Metrics comparison for A/B variants
- [ ] Graceful agent shutdown
- [ ] Agent state inspector
- [ ] Performance profiler

### Nice to Have 💡
- [ ] Multi-strategy routing (beyond A/B)
- [ ] Predictive auto-scaling (ML-based)
- [ ] Interactive debugger (GDB-like)
- [ ] Time-travel debugging (replay + inspect)
- [ ] Flame graph visualization

---

## Test Plan

### E2E Tests (Phase 10)

1. **CrashRecovery** (Priority: CRITICAL)
   - Send 100 messages
   - Crash after 50 processed
   - Restart system
   - Verify remaining 50 processed (no duplicates)

2. **DistributedTracing** (Priority: HIGH)
   - Send message: Chief → Component → Module → Task
   - Verify 4 spans created
   - Export to Jaeger
   - Visualize in Jaeger UI

3. **ABTestingExperiment** (Priority: HIGH)
   - Create 2 strategies (baseline, experimental)
   - Split traffic 50/50
   - Process 1000 messages
   - Compare metrics
   - Verify winner selected

4. **AutoScaling** (Priority: HIGH)
   - Start with 2 agents
   - Overload queue (1000 messages)
   - Verify agents scale up to 10
   - Drain queue
   - Verify agents scale down to 2

5. **MessageReplay** (Priority: MEDIUM)
   - Record 100 messages
   - Replay messages from timestamp range
   - Verify same results produced

---

## Performance Expectations

**Persistence Overhead**:
- RocksDB write latency: < 5 ms (p99)
- WAL append latency: < 1 ms (p99)
- Crash recovery time: < 10 seconds (1M messages)

**Tracing Overhead**:
- Span creation: < 100 µs
- Trace context propagation: < 50 µs
- Total tracing overhead: < 5% latency increase

**A/B Testing Overhead**:
- Strategy selection: < 10 µs
- Metrics recording: < 50 µs

**Auto-Scaling**:
- Agent spawn time: < 1 second
- Scale decision latency: < 5 seconds
- Graceful shutdown time: < 30 seconds

---

## Risk Mitigation

### Risk 1: Persistence Performance Overhead
**Impact**: Medium
**Likelihood**: Medium
**Mitigation**: Async writes, batching, tuning RocksDB settings.

### Risk 2: Tracing Overhead
**Impact**: Low
**Likelihood**: Low
**Mitigation**: Sampling (trace 10% of messages), async export.

### Risk 3: Auto-Scaling Thrashing
**Impact**: Medium
**Likelihood**: Medium
**Mitigation**: Cooldown period, hysteresis (buffer before scaling).

### Risk 4: A/B Testing Statistical Significance
**Impact**: Low
**Likelihood**: Medium
**Mitigation**: Require minimum sample size (1000+ requests) before comparison.

---

## Dependencies

### New Dependencies

1. **RocksDB** - Embedded key-value store
   - Version: 7.0+
   - Purpose: Message persistence

2. **OpenTelemetry C++** - Distributed tracing
   - Version: 1.8+
   - Purpose: Tracing infrastructure

3. **Jaeger** - Trace visualization
   - Version: 1.40+
   - Purpose: Trace backend

4. **cpp-statistics** - Statistical analysis (optional)
   - Purpose: A/B test significance testing

---

## Next Steps

1. **Week 1-2**: Message persistence
2. **Week 3-4**: Distributed tracing
3. **Week 5**: A/B testing framework
4. **Week 6-7**: Auto-scaling
5. **Week 8**: Advanced debugging

**After Phase 10**: Production deployment with full feature set

---

**Status**: 📝 Planning - Ready for Implementation
**Total Estimated Time**: 254 hours (~8 weeks)
**Last Updated**: 2025-11-19
