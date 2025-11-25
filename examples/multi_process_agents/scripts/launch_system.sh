#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../build"

# Clean up any previous runs
rm -f /tmp/keystone_queues.sock
pkill -f process1_chief || true
pkill -f process2_component_lead || true
pkill -f process3_module_lead || true
pkill -f process4_task_agent || true

sleep 1

echo "=== Starting Multi-Process Agent System ==="
echo ""

# Create logs directory
mkdir -p "$SCRIPT_DIR/../logs"

# Start background processes
echo "Starting ComponentLead (Process 2)..."
"$BUILD_DIR/process2_component_lead" > "$SCRIPT_DIR/../logs/component_lead.log" 2>&1 &
COMP_PID=$!
echo "  PID: $COMP_PID"

echo "Starting ModuleLead (Process 3)..."
"$BUILD_DIR/process3_module_lead" > "$SCRIPT_DIR/../logs/module_lead.log" 2>&1 &
MOD_PID=$!
echo "  PID: $MOD_PID"

echo "Starting TaskAgent (Process 4)..."
"$BUILD_DIR/process4_task_agent" > "$SCRIPT_DIR/../logs/task_agent.log" 2>&1 &
TASK_PID=$!
echo "  PID: $TASK_PID"

echo ""
echo "Starting ChiefArchitect (Process 1)..."
# ChiefArchitect runs in foreground
"$BUILD_DIR/process1_chief" "$1"

echo ""
echo "=== System Complete ==="
echo ""
echo "Process logs available in:"
echo "  - logs/component_lead.log"
echo "  - logs/module_lead.log"
echo "  - logs/task_agent.log"

# Cleanup background processes
echo ""
echo "Cleaning up background processes..."
kill $COMP_PID $MOD_PID $TASK_PID 2>/dev/null || true
wait $COMP_PID $MOD_PID $TASK_PID 2>/dev/null || true
echo "Done"
