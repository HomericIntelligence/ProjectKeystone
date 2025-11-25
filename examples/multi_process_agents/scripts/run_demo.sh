#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=========================================="
echo "Multi-Process Agent System Demo"
echo "=========================================="
echo ""
echo "This demo shows 4 processes coordinating to review a PR:"
echo "  - Process 1: ChiefArchitect (orchestrates)"
echo "  - Process 2: ComponentLead (domain coordination)"
echo "  - Process 3: ModuleLead (module coordination + agent selection)"
echo "  - Process 4: TaskAgent (executes specialized agents)"
echo ""
echo "Demo PR: Multi-process agent system implementation"
echo "Files: 7 files across 3 domains"
echo ""
echo "Press Enter to start..."
read

# Launch system with sample PR
"$SCRIPT_DIR/launch_system.sh" "$SCRIPT_DIR/sample_pr.json"

echo ""
echo "=========================================="
echo "Demo Complete!"
echo "=========================================="
echo ""
echo "What just happened:"
echo "1. ChiefArchitect decomposed the PR into 3 domains"
echo "2. ComponentLead (Process 2) stole domains and created module tasks"
echo "3. ModuleLead (Process 3) selected appropriate review agents based on file types"
echo "4. TaskAgent (Process 4) executed simulated Cline agents"
echo "5. Results aggregated back up through the hierarchy"
echo ""
echo "Check logs/ directory for detailed execution traces"
