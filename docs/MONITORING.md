# ProjectKeystone Monitoring Guide

**Prometheus Integration for HMAS Metrics**

## Overview

ProjectKeystone HMAS exports metrics in Prometheus format via an HTTP endpoint on port 9090. This document describes the available metrics, how to deploy Prometheus, and how to query metrics.

---

## Available Metrics

### Message Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `hmas_messages_total` | Counter | `priority` | Total messages sent by priority (high/normal/low) |
| `hmas_messages_processed_total` | Counter | - | Total messages processed |
| `hmas_messages_per_second` | Gauge | - | Current message throughput |

### Latency Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `hmas_message_latency_microseconds` | Gauge | - | Average message processing latency (µs) |

### Queue Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `hmas_queue_depth_max` | Gauge | - | Maximum queue depth across all agents |

### Worker Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `hmas_worker_utilization_percent` | Gauge | - | Worker utilization percentage (0-100) |

### Deadline Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `hmas_deadline_misses_total` | Counter | - | Total number of deadline misses |
| `hmas_deadline_miss_milliseconds` | Gauge | - | Average deadline miss time (ms) |

### System Metrics

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `hmas_uptime_seconds` | Gauge | - | HMAS uptime in seconds |
| `hmas_up` | Gauge | - | Health status (1=up, 0=down) |

---

## Deployment Options

### Option 1: Standalone Prometheus (Recommended for Testing)

Deploy Prometheus using the provided Kubernetes manifests:

```bash
# Deploy Prometheus
kubectl apply -f k8s/prometheus.yaml

# Verify deployment
kubectl get pods -n projectkeystone -l app=prometheus

# Port-forward to access Prometheus UI
kubectl port-forward -n projectkeystone svc/prometheus 9091:9090

# Open in browser
open http://localhost:9091
```

**What This Creates**:
- Prometheus deployment (1 replica)
- ConfigMap with scrape configuration
- Service for Prometheus UI
- RBAC permissions for service discovery

### Option 2: Prometheus Operator (Recommended for Production)

If using Prometheus Operator, deploy the ServiceMonitor:

```bash
# Install Prometheus Operator (if not already installed)
helm install prometheus-operator prometheus-community/kube-prometheus-stack \
  --namespace monitoring \
  --create-namespace

# Deploy ServiceMonitor
kubectl apply -f k8s/servicemonitor.yaml

# Prometheus Operator will automatically discover and scrape HMAS metrics
```

### Option 3: External Prometheus

If running Prometheus outside Kubernetes, add this scrape config to `/etc/prometheus/prometheus.yml`:

```yaml
scrape_configs:
  - job_name: 'projectkeystone-hmas'
    static_configs:
      - targets: ['<hmas-service-ip>:9090']
        labels:
          app: 'hmas'
```

---

## Accessing Metrics

### Direct HTTP Access

Test metrics endpoint directly:

```bash
# Port-forward HMAS metrics port
kubectl port-forward -n projectkeystone svc/hmas 9090:9090

# Fetch metrics
curl http://localhost:9090/metrics
```

**Expected Output**:
```
# HELP hmas_messages_total Total number of messages sent by priority
# TYPE hmas_messages_total counter
hmas_messages_total{priority="high"} 1523
hmas_messages_total{priority="normal"} 8471
hmas_messages_total{priority="low"} 2947

# HELP hmas_messages_processed_total Total number of messages processed
# TYPE hmas_messages_processed_total counter
hmas_messages_processed_total 12941

# HELP hmas_message_latency_microseconds Average message processing latency
# TYPE hmas_message_latency_microseconds gauge
hmas_message_latency_microseconds 342.5

... (more metrics)
```

### Prometheus UI

Access Prometheus UI to query and visualize metrics:

```bash
# Port-forward Prometheus UI
kubectl port-forward -n projectkeystone svc/prometheus 9091:9090

# Open in browser
open http://localhost:9091
```

Navigate to:
- **Graph**: Query and visualize metrics
- **Targets**: View scrape targets and health
- **Alerts**: View active alerts (if configured)

---

## Example Queries (PromQL)

### Message Throughput

```promql
# Messages per second (5-minute rate)
rate(hmas_messages_processed_total[5m])

# Messages per second by priority
rate(hmas_messages_total[5m])

# Total messages sent in last hour
increase(hmas_messages_total[1h])
```

### Latency

```promql
# Current average latency (microseconds)
hmas_message_latency_microseconds

# Latency over time (5-minute average)
avg_over_time(hmas_message_latency_microseconds[5m])

# Latency in milliseconds
hmas_message_latency_microseconds / 1000
```

### Queue Depth

```promql
# Maximum queue depth
hmas_queue_depth_max

# Queue depth over time
hmas_queue_depth_max

# Alert if queue depth > 1000
hmas_queue_depth_max > 1000
```

### Worker Utilization

```promql
# Current worker utilization
hmas_worker_utilization_percent

# Average utilization over last 5 minutes
avg_over_time(hmas_worker_utilization_percent[5m])

# Alert if utilization > 90%
hmas_worker_utilization_percent > 90
```

### Deadline Misses

```promql
# Deadline miss rate (per second)
rate(hmas_deadline_misses_total[5m])

# Total deadline misses in last hour
increase(hmas_deadline_misses_total[1h])

# Average miss time
hmas_deadline_miss_milliseconds
```

### System Health

```promql
# Uptime in hours
hmas_uptime_seconds / 3600

# Health status (1=up, 0=down)
hmas_up

# Alert if HMAS is down
hmas_up == 0
```

---

## Alerting Rules

Create alerting rules in Prometheus for operational monitoring.

**Example**: `monitoring/alerts.yml`

```yaml
groups:
- name: hmas_alerts
  interval: 30s
  rules:
  # High queue depth
  - alert: HMASHighQueueDepth
    expr: hmas_queue_depth_max > 1000
    for: 5m
    labels:
      severity: warning
    annotations:
      summary: "HMAS queue depth is high"
      description: "Queue depth is {{ $value }}, threshold is 1000"

  # Critical queue depth
  - alert: HMASCriticalQueueDepth
    expr: hmas_queue_depth_max > 10000
    for: 2m
    labels:
      severity: critical
    annotations:
      summary: "HMAS queue depth is critical"
      description: "Queue depth is {{ $value }}, threshold is 10000"

  # High worker utilization
  - alert: HMASHighWorkerUtilization
    expr: hmas_worker_utilization_percent > 90
    for: 10m
    labels:
      severity: warning
    annotations:
      summary: "HMAS worker utilization is high"
      description: "Worker utilization is {{ $value }}%, consider scaling up"

  # High latency
  - alert: HMASHighLatency
    expr: hmas_message_latency_microseconds > 5000
    for: 5m
    labels:
      severity: warning
    annotations:
      summary: "HMAS message latency is high"
      description: "Latency is {{ $value }}µs ({{ humanize $value | div 1000 }}ms)"

  # HMAS down
  - alert: HMASDown
    expr: hmas_up == 0
    for: 1m
    labels:
      severity: critical
    annotations:
      summary: "HMAS is down"
      description: "HMAS health check failed"

  # High deadline miss rate
  - alert: HMASHighDeadlineMissRate
    expr: rate(hmas_deadline_misses_total[5m]) > 10
    for: 5m
    labels:
      severity: warning
    annotations:
      summary: "HMAS deadline miss rate is high"
      description: "Deadline misses: {{ $value }}/sec"
```

**Deploy Alerts**:

```bash
# Add rules to Prometheus ConfigMap
kubectl create configmap prometheus-rules \
  --from-file=alerts.yml \
  --namespace=projectkeystone

# Update Prometheus to load rules
# (Requires restart or reload)
```

---

## Grafana Dashboards

### Deploying Grafana

```bash
# Install Grafana via Helm
helm install grafana grafana/grafana \
  --namespace projectkeystone \
  --set adminPassword=admin

# Port-forward to access
kubectl port-forward -n projectkeystone svc/grafana 3000:80

# Open in browser
open http://localhost:3000
# Login: admin / admin
```

### Add Prometheus Data Source

1. Navigate to **Configuration** → **Data Sources**
2. Click **Add data source**
3. Select **Prometheus**
4. Set URL: `http://prometheus:9090`
5. Click **Save & Test**

### Example Dashboard Panels

**Messages Per Second**:
```promql
rate(hmas_messages_processed_total[5m])
```

**Latency Over Time**:
```promql
hmas_message_latency_microseconds / 1000
```

**Queue Depth**:
```promql
hmas_queue_depth_max
```

**Worker Utilization**:
```promql
hmas_worker_utilization_percent
```

---

## Troubleshooting

### Metrics Endpoint Not Responding

```bash
# Check if HMAS pods are running
kubectl get pods -n projectkeystone -l app=hmas

# Check pod logs
kubectl logs -n projectkeystone <pod-name>

# Test metrics endpoint directly
kubectl exec -n projectkeystone <pod-name> -- curl localhost:9090/metrics
```

### Prometheus Not Scraping HMAS

```bash
# Check Prometheus targets
kubectl port-forward -n projectkeystone svc/prometheus 9091:9090
open http://localhost:9091/targets

# Check if HMAS service is accessible from Prometheus pod
kubectl exec -n projectkeystone <prometheus-pod> -- wget -O- http://hmas:9090/metrics
```

**Common Issues**:
- Service selector doesn't match pods
- Prometheus annotations missing on pods
- Network policy blocking traffic
- RBAC permissions missing

### No Metrics Data

```bash
# Verify PrometheusExporter is started
# Check HMAS logs for:
# "Prometheus exporter started on port 9090"

# Test metrics generation
kubectl exec -n projectkeystone <pod-name> -- curl localhost:9090/metrics
```

### High Cardinality Warnings

If you see warnings about high cardinality:
- Avoid adding labels with many unique values (e.g., msg_id, timestamps)
- Use aggregations instead of per-message metrics
- Consider using histograms for distributions

---

## Integration with Phase 6.4 (Grafana)

Phase 6.4 will create pre-built Grafana dashboards:
- **HMAS Overview** - Key metrics at a glance
- **Agent Details** - Per-agent performance
- **System Health** - Infrastructure monitoring

Stay tuned for Phase 6.4!

---

## Configuration

### Adjust Scrape Interval

Edit `monitoring/prometheus.yml`:

```yaml
global:
  scrape_interval: 15s  # Increase frequency (default: 30s)
```

### Data Retention

Edit Prometheus deployment args:

```yaml
args:
  - '--storage.tsdb.retention.time=30d'  # Increase from 15d
```

### Resource Limits

Adjust Prometheus resources in `k8s/prometheus.yaml`:

```yaml
resources:
  limits:
    cpu: 2000m      # Increase if needed
    memory: 2Gi     # Increase if needed
```

---

## Performance Considerations

### Metrics Export Overhead

- **HTTP server**: Minimal overhead (~1ms per request)
- **Metric collection**: Lock-free atomics (negligible)
- **Scrape frequency**: 30s default (balance freshness vs load)

### Storage Requirements

**Per metric**:
- ~1-2 bytes per data point
- 30s scrape interval = 2 data points/minute
- 15 days retention = ~43k data points/metric

**Total** (20 metrics):
- ~20 metrics × 43k points × 2 bytes = ~1.7 MB
- Plus overhead: ~5-10 MB total

### Scaling Considerations

- Prometheus can handle 10M+ samples/sec
- HMAS metrics: ~20 samples every 30s = negligible load
- No special tuning needed for current scale

---

## Next Steps

### Phase 6.4: Grafana Dashboards

Coming next:
- Pre-built Grafana dashboards
- Visual metrics exploration
- Real-time monitoring
- Alerting integration

### Phase 6.5: Centralized Logging

After Grafana:
- Loki log aggregation
- Structured logging
- Log correlation with metrics

---

**Phase**: 6.3 - Prometheus Monitoring
**Last Updated**: 2025-11-19
**Status**: Complete ✅
