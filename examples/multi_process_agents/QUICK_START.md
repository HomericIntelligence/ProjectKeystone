# Quick Start Guide - Multi-Process Agent System

## Build and Run (One Command)

```bash
cd examples/multi_process_agents
mkdir -p build && cd build
cmake .. && make -j$(nproc)
cd ..
./scripts/run_demo.sh
```

## What Just Happened?

The demo shows 4 processes coordinating to review a PR:

1. **ChiefArchitect** (Process 1) - Reads PR, generates request_id, decomposes into domains
2. **ComponentLead** (Process 2) - Steals domains, decomposes into modules
3. **ModuleLead** (Process 3) - Steals modules, selects agents, creates tasks
4. **TaskAgent** (Process 4) - Steals tasks, executes agents, returns results

## Custom PR Review

```bash
# Create your PR JSON
cat > my_pr.json <<EOF
{
  "pr_id": "456",
  "title": "My Custom PR",
  "files": ["src/foo.cpp", "src/bar.cpp", "README.md"],
  "domains": [
    {
      "name": "implementation",
      "files": ["src/foo.cpp", "src/bar.cpp"],
      "modules": ["foo", "bar"]
    }
  ]
}
EOF

# Run with your PR
./scripts/launch_system.sh my_pr.json
```

## Check Logs

```bash
# Process logs
cat logs/component_lead.log
cat logs/module_lead.log
cat logs/task_agent.log
```

## Architecture

```
Process 1: ChiefArchitect
    ↓ /tmp/keystone_domain_queue.dat
Process 2: ComponentLead
    ↓ /tmp/keystone_module_queue.dat
Process 3: ModuleLead
    ↓ /tmp/keystone_task_queue.dat
Process 4: TaskAgent
```

## Request ID Tracking

Format: `YYYYMMDD_HHMMSS_<uuid>`

Example: `20251124_220748_5f7ca82e`

The request_id propagates through all 4 processes and is visible in all logs.

---

**Status**: Demo-ready ✅
