# ProjectKeystone - Hierarchical Multi-Agent System (HMAS)

**C++20-based hierarchical agent system implementing TDD principles**

[![Quality Gates](https://github.com/mvillmow/ProjectKeystone/actions/workflows/quality.yml/badge.svg)](https://github.com/mvillmow/ProjectKeystone/actions/workflows/quality.yml)
[![Code Coverage](https://img.shields.io/badge/coverage-86.2%25-yellow)](https://github.com/mvillmow/ProjectKeystone/actions/workflows/quality.yml)
[![Tests](https://img.shields.io/badge/tests-329%20passing-success)](.github/workflows/quality.yml)
[![C++ Standard](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

## Overview

ProjectKeystone is a high-performance, hierarchical multi-agent system (HMAS) built with modern C++20. It implements a 4-layer agent architecture with message-passing communication, work-stealing task scheduling, and comprehensive resilience features.

### Key Features

- **4-Layer Agent Hierarchy**: ChiefArchitect → ComponentLead → ModuleLead → TaskAgent
- **Async Work-Stealing Scheduler**: High-performance task distribution across worker threads
- **Message Bus Architecture**: Decoupled agent communication with routing
- **Resilience Components**: Retry policies, circuit breakers, heartbeat monitoring
- **Zero-Copy Serialization**: Efficient message passing with Cista
- **Distributed Simulation**: NUMA-aware distributed work-stealing
- **Comprehensive Testing**: 329+ unit and E2E tests (98.8% passing)
- **Performance Benchmarks**: 45+ benchmark tests across 5 suites
- **Fuzz Testing**: 4 libFuzzer targets for robustness
- **Static Analysis**: clang-tidy and cppcheck integration

## Build Requirements

- **C++20 Compiler**: GCC 13+ or Clang 15+
- **CMake**: 3.20+
- **Ninja**: Build system
- **Docker**: (Optional) For containerized builds

## Quick Start

### Build and Test

```bash
# Configure
mkdir build && cd build
cmake -G Ninja ..

# Build
ninja

# Run tests
ctest --output-on-failure
```

### Docker Build

```bash
# Build and run tests
docker-compose up test

# Development environment
docker-compose up -d dev
docker-compose exec dev bash
```

## Project Structure

```
ProjectKeystone/
├── include/            # Public headers
│   ├── agents/         # Agent hierarchy
│   ├── core/           # Core messaging and resilience
│   └── concurrency/    # Work-stealing scheduler
├── src/                # Implementation
├── tests/              # Unit and E2E tests
│   ├── unit/           # Component unit tests
│   └── e2e/            # End-to-end integration tests
├── benchmarks/         # Performance benchmarks
├── fuzz/               # Fuzz test targets
├── scripts/            # Automation scripts
└── docs/               # Documentation

## Quality Metrics

| Metric | Status | Target |
|--------|--------|--------|
| **Line Coverage** | 86.2% | ≥95% |
| **Function Coverage** | 88.5% | ≥90% |
| **Tests Passing** | 325/329 (98.8%) | 100% |
| **Benchmark Suites** | 5 suites, 45+ tests | - |
| **Fuzz Targets** | 4 targets | - |
| **Static Analysis** | clang-tidy + cppcheck | 0 errors |

See [Coverage Baseline](docs/plan/PHASE_9_COVERAGE_BASELINE.md) for detailed metrics.

## Testing

### Unit and E2E Tests

```bash
cd build

# Run all tests
ctest --output-on-failure

# Run specific test suite
./phase1_e2e_tests
./unit_tests
./concurrency_unit_tests
```

### Code Coverage

```bash
./scripts/generate_coverage.sh

# View report
open build/coverage/html/index.html
```

### Fuzz Testing

```bash
# Build with fuzzing
cmake -DENABLE_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++ ..
ninja

# Run fuzz targets
./fuzz_message_serialization -max_len=4096 -runs=1000000
./fuzz_message_bus_routing -max_len=4096 -runs=1000000
```

See [Fuzz Testing README](fuzz/README.md) for details.

### Performance Benchmarks

```bash
# Build in Release mode
cmake -DCMAKE_BUILD_TYPE=Release ..
ninja

# Run benchmarks
./scripts/run_benchmarks.sh

# Save baseline
./scripts/run_benchmarks.sh --baseline

# Detect regressions
./scripts/run_benchmarks.sh --compare benchmarks/results/baseline.json
```

See [Benchmarks README](benchmarks/README.md) for details.

### Static Analysis

```bash
./scripts/run_static_analysis.sh

# View reports
cat build/static_analysis/clang-tidy-report.txt
cat build/static_analysis/cppcheck-report.txt
```

## CI/CD Quality Gates

ProjectKeystone enforces 5 quality gates in CI/CD:

1. **Code Coverage** (≥95%)
2. **Static Analysis** (0 critical errors)
3. **Fuzz Testing** (1 hour, no crashes)
4. **Performance Benchmarks** (no >10% regressions)
5. **Unit & E2E Tests** (all passing)

See [CI/CD Quality Gates](docs/CICD_QUALITY_GATES.md) for complete documentation.

## Development Phases

ProjectKeystone follows a phased TDD approach:

- **Phase 1** ✅: Basic delegation (L0 → L3)
- **Phase 2** ✅: Module coordination (L0 → L2 → L3)
- **Phase 3** ✅: Full 4-layer hierarchy (L0 → L1 → L2 → L3)
- **Phase 4** ✅: Multi-component system
- **Phase 5** ✅: Chaos engineering and resilience
- **Phase D** ✅: Performance optimizations (profiling, pooling, distributed)
- **Phase 9** 🚧: Enhanced testing and quality (in progress)

Current status: **Phase 9** (Enhanced Testing & Quality)

See [TDD Roadmap](docs/plan/TDD_FOUR_LAYER_ROADMAP.md) for detailed phase descriptions.

## Architecture

### Agent Hierarchy

```
ChiefArchitectAgent (L0)
    │
    ├─── ComponentLeadAgent (L1)
    │        │
    │        └─── ModuleLeadAgent (L2)
    │                 │
    │                 └─── TaskAgent (L3)
```

### Core Components

- **MessageBus**: Central routing for agent communication
- **WorkStealingScheduler**: Thread pool with task stealing
- **RetryPolicy**: Exponential backoff for transient failures
- **CircuitBreaker**: Fast failure for persistent errors
- **HeartbeatMonitor**: Agent liveness tracking
- **MessagePool**: Zero-allocation message pooling

See [Architecture Docs](docs/plan/FOUR_LAYER_ARCHITECTURE.md) for details.

## Documentation

- [CLAUDE.md](CLAUDE.md) - Project overview for Claude Code
- [Four-Layer Architecture](docs/plan/FOUR_LAYER_ARCHITECTURE.md)
- [TDD Roadmap](docs/plan/TDD_FOUR_LAYER_ROADMAP.md)
- [Testing Strategy](docs/plan/testing-strategy.md)
- [CI/CD Quality Gates](docs/CICD_QUALITY_GATES.md)
- [Coverage Baseline](docs/plan/PHASE_9_COVERAGE_BASELINE.md)
- [Phase 5 Complete](docs/plan/PHASE_5_COMPLETE.md)

## Performance Targets

| Component | Metric | Target | Current |
|-----------|--------|--------|---------|
| MessageBus | Routing Latency | <500ns | TBD |
| MessageBus | Throughput | >2M msg/sec | TBD |
| Message Pool | Allocations | >10M/sec | TBD |
| Work-Stealing | Local Steal | <10us | TBD |
| Work-Stealing | Remote Steal | <100us | TBD |

Run benchmarks to establish baselines: `./scripts/run_benchmarks.sh --baseline`

## Contributing

1. Follow TDD approach (test first)
2. Maintain ≥95% code coverage
3. Pass all quality gates
4. Update documentation

See [CLAUDE.md](CLAUDE.md) for development guidelines.

## License

MIT License - See [LICENSE](LICENSE) for details.

## Acknowledgments

Built with:
- [Google Test](https://github.com/google/googletest) - Unit testing framework
- [Google Benchmark](https://github.com/google/benchmark) - Performance benchmarking
- [spdlog](https://github.com/gabime/spdlog) - Fast logging library
- [Cista](https://github.com/felixguendling/cista) - Zero-copy serialization
- [concurrentqueue](https://github.com/cameron314/concurrentqueue) - Lock-free queue

---

**Status**: Phase 9 (Enhanced Testing & Quality) - 86.2% coverage, 329 tests
**Last Updated**: 2025-11-19
