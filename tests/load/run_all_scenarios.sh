#!/bin/bash
# run_all_scenarios.sh
# Run all HMAS load test scenarios for Phase 6.7 M1
#
# Usage:
#   ./tests/load/run_all_scenarios.sh
#   ./tests/load/run_all_scenarios.sh --quick  # Run shorter duration for CI

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

PROJECT_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
RESULTS_DIR="$PROJECT_ROOT/benchmarks/load_test_results"
LOAD_TEST_BIN="$BUILD_DIR/hmas_load_test"

# Parse arguments
QUICK_MODE=false
if [[ "$1" == "--quick" ]]; then
    QUICK_MODE=true
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

echo -e "${BLUE}=== ProjectKeystone HMAS Load Test Suite ===${NC}"
echo ""

# Check if binary exists
if [[ ! -f "$LOAD_TEST_BIN" ]]; then
    echo -e "${RED}Error: Load test binary not found${NC}"
    echo "Build with: cd build && cmake -DCMAKE_BUILD_TYPE=Release -G Ninja .. && ninja hmas_load_test"
    exit 1
fi

# Test scenarios
declare -a SCENARIOS=(
    "sustained_load"
    "burst"
    "large_hierarchy"
    "priority_fairness"
    # "chaos"  # Disabled - requires failure injection
)

# Duration for each scenario
if [[ "$QUICK_MODE" == "true" ]]; then
    SUSTAINED_DURATION=60
    BURST_DURATION=30
    HIERARCHY_DURATION=30
    PRIORITY_DURATION=30
    echo -e "${YELLOW}Running in QUICK mode (reduced duration)${NC}"
    echo ""
else
    SUSTAINED_DURATION=600
    BURST_DURATION=300
    HIERARCHY_DURATION=300
    PRIORITY_DURATION=180
fi

# Timestamp for this test run
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Track results
PASSED=0
FAILED=0
declare -a FAILED_SCENARIOS=()

# Run each scenario
for scenario in "${SCENARIOS[@]}"; do
    echo -e "${BLUE}=== Running scenario: $scenario ===${NC}"

    # Set duration based on scenario
    case $scenario in
        sustained_load)
            DURATION=$SUSTAINED_DURATION
            MESSAGE_RATE=100
            ;;
        burst)
            DURATION=$BURST_DURATION
            MESSAGE_RATE=500
            ;;
        large_hierarchy)
            DURATION=$HIERARCHY_DURATION
            MESSAGE_RATE=200
            ;;
        priority_fairness)
            DURATION=$PRIORITY_DURATION
            MESSAGE_RATE=300
            ;;
        *)
            DURATION=300
            MESSAGE_RATE=200
            ;;
    esac

    OUTPUT_FILE="$RESULTS_DIR/${scenario}_${TIMESTAMP}.json"

    # Run load test
    if $LOAD_TEST_BIN \
        --scenario=$scenario \
        --duration=$DURATION \
        --message-rate=$MESSAGE_RATE \
        --output=$OUTPUT_FILE; then
        echo -e "${GREEN}✓ $scenario passed${NC}"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}✗ $scenario failed${NC}"
        FAILED=$((FAILED + 1))
        FAILED_SCENARIOS+=("$scenario")
    fi

    echo ""
done

# Summary
echo -e "${BLUE}=== Load Test Suite Summary ===${NC}"
echo ""
echo -e "  ${GREEN}Passed: $PASSED${NC}"
echo -e "  ${RED}Failed: $FAILED${NC}"
echo ""

if [[ $FAILED -gt 0 ]]; then
    echo -e "${RED}Failed scenarios:${NC}"
    for scenario in "${FAILED_SCENARIOS[@]}"; do
        echo -e "  - $scenario"
    done
    echo ""
    exit 1
else
    echo -e "${GREEN}✓ All load test scenarios passed${NC}"
    echo ""
    echo "Results saved to: $RESULTS_DIR"
    echo ""
    echo "Next steps:"
    echo "  1. Analyze results: cat $RESULTS_DIR/*_${TIMESTAMP}.json | jq"
    echo "  2. Update resource sizing in docs/KUBERNETES_DEPLOYMENT.md"
    echo "  3. Configure HPA thresholds based on throughput"
    echo ""
    exit 0
fi
