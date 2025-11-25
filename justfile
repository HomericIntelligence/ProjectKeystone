# ProjectKeystone Justfile
# Unified build, test, and lint interface for both Docker and native execution
#
# Usage:
#   just --list              # List all available commands
#   just build-asan          # Build with AddressSanitizer (Docker)
#   just test-asan           # Run tests with ASan (Docker)
#   just native-build-asan   # Build with ASan (native/host)
#   NATIVE=1 just build-asan # Alternative native mode
#
# Docker mode (default): Runs commands in docker-compose dev container
# Native mode: Set NATIVE=1 or use native-* recipes

# Default recipe - show available commands
default:
    @just --list

# ============================================================================
# Configuration Variables
# ============================================================================

# Build modes supported
build_modes := "debug release asan tsan"

# Test executables
test_basic := "basic_delegation_tests"
test_module := "module_coordination_tests"
test_component := "component_coordination_tests"
test_async := "async_delegation_tests"
test_distributed := "distributed_hierarchy_tests"
test_unit := "unit_tests"
test_concurrency := "concurrency_unit_tests"
test_simulation := "simulation_unit_tests"
test_grpc := "distributed_grpc_tests"

# Docker images
docker_dev_image := "projectkeystone-dev:latest"
docker_test_image := "projectkeystone:latest"

# ============================================================================
# Docker Management
# ============================================================================

# Build Docker images (default: dev)
docker-build TARGET='dev':
    @echo "Building Docker image: {{TARGET}}..."
    docker-compose build {{TARGET}}

# Start dev container (idempotent)
docker-up:
    #!/usr/bin/env bash
    set -e
    if ! docker-compose ps dev | grep -q "Up"; then
        echo "Starting dev container..."
        docker-compose up -d dev
        echo "Waiting for container to be ready..."
        sleep 2
    fi

# Stop all containers
docker-down:
    @echo "Stopping containers..."
    docker-compose down

# Clean Docker resources (containers, volumes, images)
docker-clean:
    @echo "Cleaning Docker resources..."
    docker-compose down -v
    docker rmi -f {{docker_dev_image}} {{docker_test_image}} || true

# Enter dev container shell
docker-shell: docker-up
    docker-compose exec dev /bin/bash

# ============================================================================
# Build Recipes
# ============================================================================

# Build with AddressSanitizer + UBSan
build-asan: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Building with AddressSanitizer + UBSan..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cmake -S . -B build/asan -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g"
        cmake --build build/asan -j$(nproc)
    else
        docker-compose exec -T dev bash -c "
            cmake -S . -B build/asan -G Ninja \
                -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' &&
            cmake --build build/asan -j\$(nproc)
        "
    fi
    echo "✓ ASan build complete: build/asan/"

# Build with ThreadSanitizer
build-tsan: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Building with ThreadSanitizer..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cmake -S . -B build/tsan -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g"
        cmake --build build/tsan -j$(nproc)
    else
        docker-compose exec -T dev bash -c "
            cmake -S . -B build/tsan -G Ninja \
                -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer -g' &&
            cmake --build build/tsan -j\$(nproc)
        "
    fi
    echo "✓ TSan build complete: build/tsan/"

# Build with UndefinedBehaviorSanitizer only
build-ubsan: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Building with UndefinedBehaviorSanitizer..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cmake -S . -B build/ubsan -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer -g"
        cmake --build build/ubsan -j$(nproc)
    else
        docker-compose exec -T dev bash -c "
            cmake -S . -B build/ubsan -G Ninja \
                -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_CXX_FLAGS='-fsanitize=undefined -fno-omit-frame-pointer -g' &&
            cmake --build build/ubsan -j\$(nproc)
        "
    fi
    echo "✓ UBSan build complete: build/ubsan/"

# Build with LeakSanitizer only
build-lsan: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Building with LeakSanitizer..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cmake -S . -B build/lsan -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_FLAGS="-fsanitize=leak -fno-omit-frame-pointer -g"
        cmake --build build/lsan -j$(nproc)
    else
        docker-compose exec -T dev bash -c "
            cmake -S . -B build/lsan -G Ninja \
                -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_CXX_FLAGS='-fsanitize=leak -fno-omit-frame-pointer -g' &&
            cmake --build build/lsan -j\$(nproc)
        "
    fi
    echo "✓ LSan build complete: build/lsan/"

# Build debug mode (no sanitizers)
build-debug: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Building debug mode..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
        cmake --build build/debug -j$(nproc)
    else
        docker-compose exec -T dev bash -c "
            cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug &&
            cmake --build build/debug -j\$(nproc)
        "
    fi
    echo "✓ Debug build complete: build/debug/"

# Build release mode (optimized)
build-release: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Building release mode..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
        cmake --build build/release -j$(nproc)
    else
        docker-compose exec -T dev bash -c "
            cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release &&
            cmake --build build/release -j\$(nproc)
        "
    fi
    echo "✓ Release build complete: build/release/"

# Build with gRPC support (Phase 8 optional)
build-grpc: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Building with gRPC support..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cmake -S . -B build/grpc -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DENABLE_GRPC=ON
        cmake --build build/grpc -j$(nproc)
    else
        docker-compose exec -T dev bash -c "
            cmake -S . -B build/grpc -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DENABLE_GRPC=ON &&
            cmake --build build/grpc -j\$(nproc)
        "
    fi
    echo "✓ gRPC build complete: build/grpc/"

# Build (alias for build-debug)
build: build-debug

# Build with code coverage instrumentation
build-coverage: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Building with coverage instrumentation..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cmake -S . -B build/coverage -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DENABLE_COVERAGE=ON
        cmake --build build/coverage -j$(nproc)
    else
        docker-compose exec -T dev bash -c "
            cmake -S . -B build/coverage -G Ninja \
                -DCMAKE_BUILD_TYPE=Debug \
                -DENABLE_COVERAGE=ON &&
            cmake --build build/coverage -j\$(nproc)
        "
    fi
    echo "✓ Coverage build complete: build/coverage/"

# ============================================================================
# Test Recipes - Individual Test Suites
# ============================================================================

# Run basic delegation tests (Phase 1)
test-basic: build-asan
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/asan/{{test_basic}}
    else
        docker-compose exec -T dev ./build/asan/{{test_basic}}
    fi

# Run module coordination tests (Phase 2)
test-module: build-asan
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/asan/{{test_module}}
    else
        docker-compose exec -T dev ./build/asan/{{test_module}}
    fi

# Run component coordination tests (Phase 3)
test-component: build-asan
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/asan/{{test_component}}
    else
        docker-compose exec -T dev ./build/asan/{{test_component}}
    fi

# Run async delegation tests (Phase A)
test-async: build-asan
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/asan/{{test_async}}
    else
        docker-compose exec -T dev ./build/asan/{{test_async}}
    fi

# Run distributed hierarchy tests
test-distributed: build-asan
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/asan/{{test_distributed}}
    else
        docker-compose exec -T dev ./build/asan/{{test_distributed}}
    fi

# Run unit tests
test-unit: build-asan
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/asan/{{test_unit}}
    else
        docker-compose exec -T dev ./build/asan/{{test_unit}}
    fi

# Run concurrency unit tests
test-concurrency: build-asan
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/asan/{{test_concurrency}}
    else
        docker-compose exec -T dev ./build/asan/{{test_concurrency}}
    fi

# Run simulation unit tests
test-simulation: build-asan
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/asan/{{test_simulation}}
    else
        docker-compose exec -T dev ./build/asan/{{test_simulation}}
    fi

# Run gRPC distributed tests (requires ENABLE_GRPC)
test-grpc: build-grpc
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/grpc/{{test_grpc}}
    else
        docker-compose exec -T dev ./build/grpc/{{test_grpc}}
    fi

# ============================================================================
# Test Recipes - Aggregated
# ============================================================================

# Run all tests with ASan (default)
test-asan: build-asan
    #!/usr/bin/env bash
    set -e
    echo "Running all tests with AddressSanitizer..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cd build/asan && ctest --output-on-failure -j$(nproc)
    else
        docker-compose exec -T dev bash -c "cd build/asan && ctest --output-on-failure -j\$(nproc)"
    fi

# Run all tests with TSan
test-tsan: build-tsan
    #!/usr/bin/env bash
    set -e
    echo "Running all tests with ThreadSanitizer..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cd build/tsan && ctest --output-on-failure -j$(nproc)
    else
        docker-compose exec -T dev bash -c "cd build/tsan && ctest --output-on-failure -j\$(nproc)"
    fi

# Run all tests with UBSan
test-ubsan: build-ubsan
    #!/usr/bin/env bash
    set -e
    echo "Running all tests with UndefinedBehaviorSanitizer..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cd build/ubsan && ctest --output-on-failure -j$(nproc)
    else
        docker-compose exec -T dev bash -c "cd build/ubsan && ctest --output-on-failure -j\$(nproc)"
    fi

# Run all tests with LSan
test-lsan: build-lsan
    #!/usr/bin/env bash
    set -e
    echo "Running all tests with LeakSanitizer..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        cd build/lsan && ctest --output-on-failure -j$(nproc)
    else
        docker-compose exec -T dev bash -c "cd build/lsan && ctest --output-on-failure -j\$(nproc)"
    fi

# Run all tests (alias for test-asan)
test: test-asan

# Run all tests (explicit)
test-all: test-asan

# ============================================================================
# Test Recipes - With Filtering
# ============================================================================

# Run specific test with GTest filter
test-filter SUITE FILTER: build-asan
    #!/usr/bin/env bash
    set -e
    echo "Running {{SUITE}} with filter: {{FILTER}}"
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/asan/{{SUITE}} --gtest_filter="{{FILTER}}"
    else
        docker-compose exec -T dev ./build/asan/{{SUITE}} --gtest_filter="{{FILTER}}"
    fi

# Example: just test-filter basic_delegation_tests "E2E_Phase1.*"

# ============================================================================
# Load Testing & Benchmarks
# ============================================================================

# Run all load test scenarios
load-test: build-release
    #!/usr/bin/env bash
    set -e
    echo "Running load tests (full duration)..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./tests/load/run_all_scenarios.sh
    else
        docker-compose exec -T dev ./tests/load/run_all_scenarios.sh
    fi

# Run load tests in quick mode (for CI)
load-test-quick: build-release
    #!/usr/bin/env bash
    set -e
    echo "Running load tests (quick mode)..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./tests/load/run_all_scenarios.sh --quick
    else
        docker-compose exec -T dev ./tests/load/run_all_scenarios.sh --quick
    fi

# Run specific load test scenario
load-test-scenario SCENARIO: build-release
    #!/usr/bin/env bash
    set -e
    echo "Running load test scenario: {{SCENARIO}}..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/release/hmas_load_test --scenario={{SCENARIO}} --duration=300
    else
        docker-compose exec -T dev ./build/release/hmas_load_test --scenario={{SCENARIO}} --duration=300
    fi

# Run all benchmarks
benchmark: build-release
    #!/usr/bin/env bash
    set -e
    echo "Running benchmarks..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./scripts/run_benchmarks.sh
    else
        docker-compose exec -T dev ./scripts/run_benchmarks.sh
    fi

# Run message pool benchmarks
benchmark-message-pool: build-release
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/release/message_pool_benchmarks
    else
        docker-compose exec -T dev ./build/release/message_pool_benchmarks
    fi

# Run distributed benchmarks
benchmark-distributed: build-release
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/release/distributed_benchmarks
    else
        docker-compose exec -T dev ./build/release/distributed_benchmarks
    fi

# Run string allocation benchmarks
benchmark-strings: build-release
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./build/release/string_allocation_benchmarks
    else
        docker-compose exec -T dev ./build/release/string_allocation_benchmarks
    fi

# ============================================================================
# Linting & Static Analysis
# ============================================================================

# Run all linters (clang-tidy + cppcheck)
lint: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Running static analysis..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./scripts/run_static_analysis.sh
    else
        docker-compose exec -T dev ./scripts/run_static_analysis.sh
    fi

# Run clang-tidy only
lint-clang-tidy: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Running clang-tidy..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./scripts/run_static_analysis.sh --clang-tidy-only
    else
        docker-compose exec -T dev ./scripts/run_static_analysis.sh --clang-tidy-only
    fi

# Run cppcheck only
lint-cppcheck: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Running cppcheck..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./scripts/run_static_analysis.sh --cppcheck-only
    else
        docker-compose exec -T dev ./scripts/run_static_analysis.sh --cppcheck-only
    fi

# ============================================================================
# Code Formatting
# ============================================================================

# Format all C++ source files
format: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Formatting C++ code with clang-format..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        find src include tests benchmarks -type f \( -name "*.cpp" -o -name "*.hpp" \) \
            -not -path "*/build/*" -not -path "*/_deps/*" \
            | xargs clang-format -i
    else
        docker-compose exec -T dev bash -c \
            "find src include tests benchmarks -type f \( -name '*.cpp' -o -name '*.hpp' \) \
            -not -path '*/build/*' -not -path '*/_deps/*' \
            | xargs clang-format -i"
    fi
    echo "✓ Formatting complete"

# Check formatting without modifying files
format-check: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Checking C++ formatting..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        find src include tests benchmarks -type f \( -name "*.cpp" -o -name "*.hpp" \) \
            -not -path "*/build/*" -not -path "*/_deps/*" \
            | xargs clang-format --dry-run --Werror
    else
        docker-compose exec -T dev bash -c \
            "find src include tests benchmarks -type f \( -name '*.cpp' -o -name '*.hpp' \) \
            -not -path '*/build/*' -not -path '*/_deps/*' \
            | xargs clang-format --dry-run --Werror"
    fi
    echo "✓ Formatting check passed"

# ============================================================================
# Coverage & Profiling
# ============================================================================

# Generate code coverage report
coverage: build-coverage
    #!/usr/bin/env bash
    set -e
    echo "Generating coverage report..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        ./scripts/generate_coverage.sh
    else
        docker-compose exec -T dev ./scripts/generate_coverage.sh
    fi

# Run static analysis script
static-analysis: lint

# ============================================================================
# Clean Recipes
# ============================================================================

# Clean specific build directory
clean MODE:
    @echo "Cleaning build/{{MODE}}..."
    rm -rf build/{{MODE}}

# Clean all build directories
clean-all:
    @echo "Cleaning all build directories..."
    rm -rf build/debug build/release build/asan build/tsan build/grpc build/coverage

# Clean build artifacts and Docker resources
clean-everything: clean-all docker-clean
    @echo "Cleaned everything"

# ============================================================================
# Native Variants (run on host system instead of Docker)
# ============================================================================

# Native build variants
native-build-asan:
    @NATIVE=1 just build-asan

native-build-tsan:
    @NATIVE=1 just build-tsan

native-build-ubsan:
    @NATIVE=1 just build-ubsan

native-build-lsan:
    @NATIVE=1 just build-lsan

native-build-debug:
    @NATIVE=1 just build-debug

native-build-release:
    @NATIVE=1 just build-release

native-build-grpc:
    @NATIVE=1 just build-grpc

native-build-coverage:
    @NATIVE=1 just build-coverage

# Native test variants
native-test:
    @NATIVE=1 just test

native-test-asan:
    @NATIVE=1 just test-asan

native-test-tsan:
    @NATIVE=1 just test-tsan

native-test-ubsan:
    @NATIVE=1 just test-ubsan

native-test-lsan:
    @NATIVE=1 just test-lsan

native-test-basic:
    @NATIVE=1 just test-basic

native-test-module:
    @NATIVE=1 just test-module

native-test-component:
    @NATIVE=1 just test-component

native-test-async:
    @NATIVE=1 just test-async

native-test-unit:
    @NATIVE=1 just test-unit

# Native lint variants
native-lint:
    @NATIVE=1 just lint

native-lint-clang-tidy:
    @NATIVE=1 just lint-clang-tidy

native-lint-cppcheck:
    @NATIVE=1 just lint-cppcheck

native-format:
    @NATIVE=1 just format

native-format-check:
    @NATIVE=1 just format-check

# Native benchmark variants
native-benchmark:
    @NATIVE=1 just benchmark

native-load-test:
    @NATIVE=1 just load-test

native-load-test-quick:
    @NATIVE=1 just load-test-quick

# ============================================================================
# CI/CD Helper Recipes
# ============================================================================

# Full CI pipeline
ci: docker-up build-asan test-asan lint format-check
    @echo "✓ CI pipeline complete"

# Quick CI (for pull requests)
ci-quick: docker-up build-asan test-basic test-module test-component format-check
    @echo "✓ Quick CI complete"

# Pre-commit checks
pre-commit: format-check lint-clang-tidy test-basic
    @echo "✓ Pre-commit checks passed"

# ============================================================================
# Help & Info
# ============================================================================

# Show this help message
help:
    @echo "ProjectKeystone Justfile"
    @echo ""
    @echo "Usage: just <recipe>"
    @echo ""
    @echo "Build Commands:"
    @echo "  build                    Build (alias for build-debug)"
    @echo "  build-debug              Build debug mode"
    @echo "  build-release            Build release mode"
    @echo "  build-asan               Build with AddressSanitizer + UBSan"
    @echo "  build-tsan               Build with ThreadSanitizer"
    @echo "  build-ubsan              Build with UndefinedBehaviorSanitizer"
    @echo "  build-lsan               Build with LeakSanitizer"
    @echo "  build-grpc               Build with gRPC support"
    @echo "  build-coverage           Build with coverage instrumentation"
    @echo ""
    @echo "Test Commands:"
    @echo "  test                     Run all tests (ASan)"
    @echo "  test-asan                Run all tests with ASan"
    @echo "  test-tsan                Run all tests with TSan"
    @echo "  test-ubsan               Run all tests with UBSan"
    @echo "  test-lsan                Run all tests with LSan"
    @echo "  test-basic               Run basic delegation tests"
    @echo "  test-module              Run module coordination tests"
    @echo "  test-component           Run component coordination tests"
    @echo "  test-async               Run async delegation tests"
    @echo "  test-unit                Run unit tests"
    @echo "  test-filter SUITE FILTER Run test with GTest filter"
    @echo ""
    @echo "Load & Benchmark:"
    @echo "  load-test                Run all load tests"
    @echo "  load-test-quick          Run quick load tests (CI)"
    @echo "  benchmark                Run all benchmarks"
    @echo "  benchmark-message-pool   Run message pool benchmarks"
    @echo ""
    @echo "Lint & Format:"
    @echo "  lint                     Run all linters"
    @echo "  lint-clang-tidy          Run clang-tidy only"
    @echo "  lint-cppcheck            Run cppcheck only"
    @echo "  format                   Format all C++ files"
    @echo "  format-check             Check formatting (CI)"
    @echo ""
    @echo "Docker:"
    @echo "  docker-build [TARGET]    Build Docker image"
    @echo "  docker-up                Start dev container"
    @echo "  docker-down              Stop containers"
    @echo "  docker-shell             Enter dev container"
    @echo ""
    @echo "Clean:"
    @echo "  clean MODE               Clean specific build dir"
    @echo "  clean-all                Clean all build dirs"
    @echo ""
    @echo "Native Mode:"
    @echo "  Add 'native-' prefix or set NATIVE=1"
    @echo "  Example: just native-build-asan"
    @echo "           NATIVE=1 just build-asan"
    @echo ""
    @echo "CI/CD:"
    @echo "  ci                       Full CI pipeline"
    @echo "  ci-quick                 Quick CI for PRs"
    @echo "  pre-commit               Pre-commit checks"
