# Phase 6: Production Deployment Plan

**Status**: 📝 Planning
**Date Started**: TBD
**Target Completion**: TBD
**Branch**: TBD

## Overview

Phase 6 prepares the ProjectKeystone HMAS for **production deployment** with containerization, orchestration, and observability. The system will be deployable to Kubernetes clusters with comprehensive monitoring, health checks, and graceful shutdown capabilities.

### Current Status (Post-Phase 5)

**What We Have**:
- ✅ Full 4-layer async hierarchy (L0-L3)
- ✅ Multi-component coordination (Phase 4)
- ✅ Chaos engineering infrastructure (Phase 5)
- ✅ 329/329 tests passing
- ✅ Work-stealing scheduler with C++20 coroutines
- ✅ Distributed simulation framework

**What Phase 6 Adds**:
- Production-optimized Docker images
- Kubernetes deployment manifests
- Prometheus metrics exporter
- Grafana dashboards
- Health checks and readiness probes
- Graceful shutdown handling

---

## Phase 6 Architecture

```
┌─────────────────────────────────────────────────────┐
│ Kubernetes Cluster                                  │
│                                                     │
│  ┌────────────────────────────────────────────┐   │
│  │ StatefulSet: HMAS Agents                   │   │
│  │  - ChiefArchitect (replica: 1)             │   │
│  │  - ComponentLeads (replicas: 4)            │   │
│  │  - ModuleLeads (replicas: 12)              │   │
│  │  - TaskAgents (replicas: 50)               │   │
│  └────────────────────────────────────────────┘   │
│                                                     │
│  ┌────────────────────────────────────────────┐   │
│  │ Service: MessageBus Endpoint               │   │
│  │  - ClusterIP: internal communication       │   │
│  │  - Port: 8080 (gRPC)                       │   │
│  └────────────────────────────────────────────┘   │
│                                                     │
│  ┌────────────────────────────────────────────┐   │
│  │ Service: Metrics Endpoint                  │   │
│  │  - ClusterIP: Prometheus scraping          │   │
│  │  - Port: 9090 (HTTP)                       │   │
│  └────────────────────────────────────────────┘   │
│                                                     │
└─────────────────────────────────────────────────────┘
         │                           │
         │ Metrics                   │ Logs
         ▼                           ▼
┌──────────────────┐        ┌──────────────────┐
│ Prometheus       │        │ Loki/ELK Stack   │
│ (Metrics)        │        │ (Logs)           │
└────────┬─────────┘        └──────────────────┘
         │
         ▼
┌──────────────────┐
│ Grafana          │
│ (Dashboards)     │
└──────────────────┘
```

---

## Phase 6 Implementation Plan

### Phase 6.1: Docker Production Build (Week 1)

**Goal**: Optimize Dockerfile for production deployment

**Tasks**:

1. **Multi-stage Dockerfile Optimization** (6 hours)
   - **Builder stage**: Compile with optimizations (`-O3`, `-DNDEBUG`)
   - **Runtime stage**: Minimal base image (Alpine or distroless)
   - **Development stage**: Keep existing dev tools
   - Strip debug symbols from production binaries
   - Size target: < 50 MB runtime image

2. **Security Hardening** (4 hours)
   - Non-root user (`USER hmas:hmas`)
   - Read-only filesystem where possible
   - Minimal attack surface (no shell in runtime)
   - CVE scanning with Trivy
   - SBOM generation

3. **Health Check Endpoint** (4 hours)
   - HTTP endpoint `/health` (returns 200 OK if healthy)
   - Check MessageBus connectivity
   - Check agent registry status
   - Return JSON with component health

4. **Graceful Shutdown** (4 hours)
   - Signal handling (SIGTERM, SIGINT)
   - Drain message queues before exit
   - Save state to persistent storage (if needed)
   - Notify other agents of shutdown

**Deliverables**:
- ✅ Production Dockerfile (multi-stage)
- ✅ Security-hardened runtime image
- ✅ Health check endpoint
- ✅ Graceful shutdown handler

**Estimated Time**: 18 hours

---

### Phase 6.2: Kubernetes Manifests (Week 2)

**Goal**: Deploy HMAS to Kubernetes cluster

**Tasks**:

1. **StatefulSet for HMAS Agents** (6 hours)
   ```yaml
   apiVersion: apps/v1
   kind: StatefulSet
   metadata:
     name: hmas-chief-architect
   spec:
     serviceName: hmas
     replicas: 1
     selector:
       matchLabels:
         app: hmas
         tier: chief
     template:
       metadata:
         labels:
           app: hmas
           tier: chief
       spec:
         containers:
         - name: chief-architect
           image: projectkeystone:latest
           ports:
           - containerPort: 8080
             name: grpc
           - containerPort: 9090
             name: metrics
           livenessProbe:
             httpGet:
               path: /health
               port: 9090
             initialDelaySeconds: 10
             periodSeconds: 10
           readinessProbe:
             httpGet:
               path: /ready
               port: 9090
             initialDelaySeconds: 5
             periodSeconds: 5
   ```

2. **Service for MessageBus** (3 hours)
   - ClusterIP service for internal communication
   - Headless service for StatefulSet DNS
   - Load balancer for external access (optional)

3. **ConfigMap for Configuration** (3 hours)
   - Agent configuration (thread pool size, queue depths)
   - Logging configuration
   - Feature flags

4. **Secrets for Credentials** (2 hours)
   - API keys (if using LLMs in future)
   - Database credentials (if needed)
   - TLS certificates

5. **Persistent Volume Claims** (4 hours)
   - StatefulSet storage for agent state
   - Logs persistence
   - Metrics retention

**Deliverables**:
- ✅ StatefulSet manifests (Chief, Component, Module, Task)
- ✅ Service manifests (MessageBus, Metrics)
- ✅ ConfigMap and Secret templates
- ✅ PersistentVolumeClaim manifests

**Estimated Time**: 18 hours

---

### Phase 6.3: Prometheus Metrics Integration (Week 3)

**Goal**: Export HMAS metrics to Prometheus

**Tasks**:

1. **Prometheus C++ Client Integration** (8 hours)
   - Add `prometheus-cpp` library to CMake
   - Create `MetricsExporter` class
   - Expose metrics on `:9090/metrics`
   - Integrate with existing `Metrics` class

2. **Agent-Level Metrics** (6 hours)
   ```cpp
   // Counters
   messages_sent_total
   messages_received_total
   tasks_completed_total
   tasks_failed_total

   // Gauges
   active_agents
   queue_depth
   worker_utilization

   // Histograms
   message_latency_seconds
   task_duration_seconds
   ```

3. **System-Level Metrics** (4 hours)
   - CPU usage per agent
   - Memory usage per agent
   - Message bus throughput
   - Work-stealing steal count

4. **Prometheus Configuration** (2 hours)
   ```yaml
   scrape_configs:
     - job_name: 'hmas'
       static_configs:
         - targets: ['hmas-metrics:9090']
       scrape_interval: 10s
   ```

**Deliverables**:
- ✅ `MetricsExporter` class
- ✅ Prometheus metrics endpoint
- ✅ Agent and system metrics
- ✅ Prometheus scrape config

**Estimated Time**: 20 hours

---

### Phase 6.4: Grafana Dashboards (Week 4)

**Goal**: Visualize HMAS metrics in Grafana

**Tasks**:

1. **HMAS Overview Dashboard** (6 hours)
   - Agent hierarchy visualization (tree view)
   - Total agents by tier (L0, L1, L2, L3)
   - Messages sent/received (rate graphs)
   - Task completion rate

2. **Performance Dashboard** (6 hours)
   - Message latency (p50, p95, p99)
   - Task duration distribution
   - Worker utilization heatmap
   - Queue depth time series

3. **Health Dashboard** (4 hours)
   - Agent health status (up/down)
   - Failed tasks over time
   - Circuit breaker states
   - Heartbeat monitoring

4. **Chaos Dashboard** (4 hours)
   - Failure injection statistics
   - Network partition events
   - Retry attempts and success rate
   - Recovery time metrics

**Deliverables**:
- ✅ Grafana dashboard JSONs (4 dashboards)
- ✅ Datasource configuration
- ✅ Alert rules (optional)

**Estimated Time**: 20 hours

---

### Phase 6.5: Helm Chart (Week 5)

**Goal**: Package HMAS for easy deployment

**Tasks**:

1. **Helm Chart Structure** (4 hours)
   ```
   hmas-chart/
   ├── Chart.yaml
   ├── values.yaml
   ├── templates/
   │   ├── statefulset.yaml
   │   ├── service.yaml
   │   ├── configmap.yaml
   │   ├── secret.yaml
   │   └── ingress.yaml
   └── README.md
   ```

2. **Templating with Values** (6 hours)
   ```yaml
   # values.yaml
   replicaCount:
     chief: 1
     component: 4
     module: 12
     task: 50

   image:
     repository: projectkeystone
     tag: latest
     pullPolicy: IfNotPresent

   resources:
     chief:
       limits:
         cpu: 2000m
         memory: 2Gi
       requests:
         cpu: 1000m
         memory: 1Gi
   ```

3. **Helm Install/Upgrade Commands** (2 hours)
   ```bash
   helm install hmas ./hmas-chart --namespace hmas --create-namespace
   helm upgrade hmas ./hmas-chart --namespace hmas
   helm rollback hmas 1 --namespace hmas
   ```

**Deliverables**:
- ✅ Helm chart package
- ✅ Customizable `values.yaml`
- ✅ Installation guide

**Estimated Time**: 12 hours

---

## Success Criteria

### Must Have ✅
- [ ] Production Docker image < 50 MB
- [ ] Security hardening (non-root, CVE-free)
- [ ] Health check endpoint working
- [ ] Graceful shutdown implemented
- [ ] Kubernetes StatefulSet deployable
- [ ] Prometheus metrics exported
- [ ] Grafana dashboards functional
- [ ] Helm chart installable

### Should Have 🎯
- [ ] Multi-stage Dockerfile optimized
- [ ] ConfigMap/Secret externalization
- [ ] PersistentVolume for state
- [ ] Readiness probe working
- [ ] 4+ Grafana dashboards
- [ ] Metrics coverage > 80%

### Nice to Have 💡
- [ ] Horizontal Pod Autoscaling (HPA)
- [ ] Service mesh integration (Istio/Linkerd)
- [ ] Distributed tracing (Jaeger/Zipkin)
- [ ] Log aggregation (Loki/ELK)
- [ ] ArgoCD GitOps deployment

---

## Test Plan

### E2E Tests (Phase 6)

1. **DockerBuildProduction** (Priority: CRITICAL)
   - Build production Dockerfile
   - Image size < 50 MB
   - No vulnerabilities (Trivy scan)

2. **HealthCheckEndpoint** (Priority: HIGH)
   - HTTP GET `/health` returns 200
   - JSON response contains component status
   - Works in Kubernetes liveness probe

3. **GracefulShutdown** (Priority: HIGH)
   - Send SIGTERM to running agent
   - Message queues drain within 30s
   - No message loss
   - Clean exit (code 0)

4. **KubernetesDeployment** (Priority: CRITICAL)
   - Deploy StatefulSet to minikube
   - All pods reach Running state
   - Services expose correct ports
   - Metrics endpoint accessible

5. **PrometheusMetrics** (Priority: HIGH)
   - Prometheus scrapes metrics successfully
   - Agent-level metrics present
   - System-level metrics present
   - No scrape errors

6. **GrafanaDashboards** (Priority: MEDIUM)
   - Import dashboard JSONs
   - Graphs render with data
   - Queries return results
   - No errors in logs

---

## Performance Expectations

**Production Targets**:
- Docker image size: < 50 MB (Alpine-based)
- Startup time: < 10 seconds
- Health check latency: < 50 ms
- Graceful shutdown time: < 30 seconds
- Metrics scrape duration: < 100 ms
- Dashboard query latency: < 500 ms

**Resource Limits** (per agent):
- CPU: 500m (0.5 cores) baseline, 2000m (2 cores) limit
- Memory: 512 Mi baseline, 2 Gi limit
- Disk: 1 Gi persistent storage

---

## Risk Mitigation

### Risk 1: Docker Image Size Bloat
**Impact**: Medium
**Likelihood**: Medium
**Mitigation**: Use Alpine base, multi-stage build, strip symbols. Target < 50 MB.

### Risk 2: Kubernetes Learning Curve
**Impact**: Medium
**Likelihood**: High
**Mitigation**: Use minikube for local testing. Start with simple StatefulSet.

### Risk 3: Prometheus Metrics Overhead
**Impact**: Low
**Likelihood**: Low
**Mitigation**: prometheus-cpp is lightweight. Scrape interval 10s+ to reduce load.

### Risk 4: Grafana Dashboard Complexity
**Impact**: Low
**Likelihood**: Medium
**Mitigation**: Start with simple dashboards. Iterate based on user feedback.

---

## Implementation Notes

### Health Check Endpoint (C++)

```cpp
// include/core/health_check.hpp
class HealthCheckServer {
public:
    HealthCheckServer(int port);
    void start();
    void stop();

    struct ComponentHealth {
        std::string name;
        bool healthy;
        std::string message;
    };

    void registerComponent(const std::string& name,
                          std::function<bool()> health_check);

private:
    std::unique_ptr<httplib::Server> server_;
    std::unordered_map<std::string, std::function<bool()>> checks_;
};
```

### Graceful Shutdown Handler

```cpp
// Signal handler
std::atomic<bool> shutdown_requested{false};

void signalHandler(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        shutdown_requested.store(true);
    }
}

// In main()
signal(SIGTERM, signalHandler);
signal(SIGINT, signalHandler);

while (!shutdown_requested.load()) {
    // Process messages
}

// Drain queues
messagebus->drainQueues(std::chrono::seconds(30));
```

### Prometheus Metrics Exporter

```cpp
// include/core/metrics_exporter.hpp
class MetricsExporter {
public:
    MetricsExporter(int port);
    void start();

    // Counters
    void incrementMessagesSent();
    void incrementMessagesReceived();
    void incrementTasksCompleted();

    // Gauges
    void setActiveAgents(int count);
    void setQueueDepth(int depth);

    // Histograms
    void observeMessageLatency(double seconds);
    void observeTaskDuration(double seconds);

private:
    prometheus::Exposer exposer_;
    std::shared_ptr<prometheus::Registry> registry_;

    prometheus::Counter* messages_sent_counter_;
    prometheus::Histogram* message_latency_histogram_;
};
```

---

## Dependencies

### New Dependencies

1. **prometheus-cpp** - Prometheus C++ client library
   - Repo: https://github.com/jupp0r/prometheus-cpp
   - License: MIT
   - Purpose: Metrics exporting

2. **cpp-httplib** - HTTP server library
   - Repo: https://github.com/yhirose/cpp-httplib
   - License: MIT
   - Purpose: Health check endpoint

3. **Kubernetes** - Container orchestration
   - Version: 1.27+
   - Purpose: Production deployment

4. **Helm** - Kubernetes package manager
   - Version: 3.0+
   - Purpose: Simplified deployment

5. **Prometheus** - Metrics collection
   - Version: 2.40+
   - Purpose: Monitoring

6. **Grafana** - Metrics visualization
   - Version: 9.0+
   - Purpose: Dashboards

---

## Next Steps

1. **Week 1**: Docker production optimization
2. **Week 2**: Kubernetes manifests
3. **Week 3**: Prometheus integration
4. **Week 4**: Grafana dashboards
5. **Week 5**: Helm chart packaging

**After Phase 6**: Move to **Phase 7: AI Integration** or **Phase 9: Enhanced Testing**

---

**Status**: 📝 Planning - Ready for Implementation
**Total Estimated Time**: 88 hours (~5 weeks)
**Last Updated**: 2025-11-19
