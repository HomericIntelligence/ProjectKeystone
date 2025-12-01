# ProjectKeystone Makefile
# Simple build system with debug, release, and asan modes
# Artifacts stored in build/debug, build/release, build/debug.asan, build/release.asan
#
# Usage:
#   make                    # Build debug mode (build/debug)
#   make release            # Build release mode (build/release)
#   make debug.asan         # Build debug with ASan (build/debug.asan)
#   make release.asan       # Build release with ASan (build/release.asan)
#   make test               # Run tests (uses debug build)
#   make test.asan          # Run tests with ASan (uses debug.asan build)

# ============================================================================
# Configuration Variables
# ============================================================================

# Number of processors for parallel builds
NPROC ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Docker mode detection
ifeq ($(NATIVE),1)
	NATIVE_MODE := true
else
	NATIVE_MODE := false
endif

# Build type mappings
BUILD_TYPE_debug := Debug
BUILD_TYPE_release := Release
BUILD_TYPE_debug.asan := Debug
BUILD_TYPE_release.asan := Release

# Compiler flags
BUILD_FLAGS_debug :=
BUILD_FLAGS_release := -O3 -DNDEBUG
BUILD_FLAGS_debug.asan := -fsanitize=address,undefined -fno-omit-frame-pointer -g
BUILD_FLAGS_release.asan := -O3 -DNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer -g

# Test executables
TEST_BASIC := bin/basic_delegation_tests
TEST_MODULE := bin/module_coordination_tests
TEST_COMPONENT := bin/component_coordination_tests
TEST_ASYNC := bin/async_delegation_tests
TEST_DISTRIBUTED := bin/distributed_hierarchy_tests
TEST_UNIT := bin/unit_tests
TEST_CONCURRENCY := bin/concurrency_unit_tests
TEST_SIMULATION := bin/simulation_unit_tests
TEST_GRPC := bin/distributed_grpc_tests

# ============================================================================
# Default target
# ============================================================================

.PHONY: help

default: debug

# ============================================================================
# Build Recipes
# ============================================================================

.PHONY: debug release debug.asan release.asan

debug: build/debug/$(TEST_BASIC)
	@echo "✓ Debug build complete: build/debug/"

release: build/release/$(TEST_BASIC)
	@echo "✓ Release build complete: build/release/"

debug.asan: build/debug.asan/$(TEST_BASIC)
	@echo "✓ Debug ASan build complete: build/debug.asan/"

release.asan: build/release.asan/$(TEST_BASIC)
	@echo "✓ Release ASan build complete: build/release.asan/"

# Directory creation rule - only runs if directory doesn't exist
.PHONY: build/%/

build/%/:
	@echo "Creating build directory: build/$*"
	@mkdir -p build/$*

# Generic build rule for any mode
build/%/$(TEST_BASIC): build/%/
	@echo "Building $* mode..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		echo "  Native mode: Building on host system"; \
		cmake -S . -B build/$* -G Ninja \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE_$*) \
			$(if $(BUILD_FLAGS_$*),-DCMAKE_CXX_FLAGS="$(BUILD_FLAGS_$*)" ); \
		cmake --build build/$* -j$(NPROC); \
	else \
		echo "  Docker mode: Building in container"; \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev bash -c " \
			cmake -S . -B build/$* -G Ninja \
				-DCMAKE_BUILD_TYPE=$(BUILD_TYPE_$*) \
				$(if $(BUILD_FLAGS_$*),-DCMAKE_CXX_FLAGS=\"$(BUILD_FLAGS_$*)\" ) && \
			cmake --build build/$* -j$(NPROC)"; \
	fi

# Build all modes
.PHONY: build-all
build-all: debug release debug.asan release.asan

# Debug recipe - build and run development container
.PHONY: debug-shell
debug-shell: docker-up
	@echo "Starting development container for debugging..."
	docker-compose exec -it dev /bin/bash

# ============================================================================
# Test Recipes
# ============================================================================

.PHONY: test test-basic test-module test-component test-async test-distributed test-unit test-concurrency test-simulation test-grpc test.asan

# Test aliases - default to debug build
test: test-basic
test-basic: build/debug/$(TEST_BASIC)
	@echo "Running basic delegation tests..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug/$(TEST_BASIC); \
	else \
		docker-compose exec -T dev ./build/debug/$(TEST_BASIC); \
	fi

test-module: build/debug/$(TEST_MODULE)
	@echo "Running module coordination tests..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug/$(TEST_MODULE); \
	else \
		docker-compose exec -T dev ./build/debug/$(TEST_MODULE); \
	fi

test-component: build/debug/$(TEST_COMPONENT)
	@echo "Running component coordination tests..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug/$(TEST_COMPONENT); \
	else \
		docker-compose exec -T dev ./build/debug/$(TEST_COMPONENT); \
	fi

test-async: build/debug/$(TEST_ASYNC)
	@echo "Running async delegation tests..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug/$(TEST_ASYNC); \
	else \
		docker-compose exec -T dev ./build/debug/$(TEST_ASYNC); \
	fi

test-distributed: build/debug/$(TEST_DISTRIBUTED)
	@echo "Running distributed hierarchy tests..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug/$(TEST_DISTRIBUTED); \
	else \
		docker-compose exec -T dev ./build/debug/$(TEST_DISTRIBUTED); \
	fi

test-unit: build/debug/$(TEST_UNIT)
	@echo "Running unit tests..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug/$(TEST_UNIT); \
	else \
		docker-compose exec -T dev ./build/debug/$(TEST_UNIT); \
	fi

test-concurrency: build/debug/$(TEST_CONCURRENCY)
	@echo "Running concurrency unit tests..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug/$(TEST_CONCURRENCY); \
	else \
		docker-compose exec -T dev ./build/debug/$(TEST_CONCURRENCY); \
	fi

test-simulation: build/debug/$(TEST_SIMULATION)
	@echo "Running simulation unit tests..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug/$(TEST_SIMULATION); \
	else \
		docker-compose exec -T dev ./build/debug/$(TEST_SIMULATION); \
	fi

test-grpc: build/release/$(TEST_GRPC)
	@echo "Running gRPC distributed tests..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/release/$(TEST_GRPC); \
	else \
		docker-compose exec -T dev ./build/release/$(TEST_GRPC); \
	fi

# Run all tests for debug build
test-all: build/debug/$(TEST_BASIC)
	@echo "Running all tests for debug mode..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		cd build/debug && ctest --output-on-failure -j$(NPROC); \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev bash -c "cd build/debug && ctest --output-on-failure -j$(NPROC)"; \
	fi

# ASan test aliases - use debug.asan build
test.asan: test-basic.asan
test-basic.asan: build/debug.asan/$(TEST_BASIC)
	@echo "Running basic delegation tests with ASan..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug.asan/$(TEST_BASIC); \
	else \
		docker-compose exec -T dev ./build/debug.asan/$(TEST_BASIC); \
	fi

test-module.asan: build/debug.asan/$(TEST_MODULE)
	@echo "Running module coordination tests with ASan..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug.asan/$(TEST_MODULE); \
	else \
		docker-compose exec -T dev ./build/debug.asan/$(TEST_MODULE); \
	fi

test-component.asan: build/debug.asan/$(TEST_COMPONENT)
	@echo "Running component coordination tests with ASan..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug.asan/$(TEST_COMPONENT); \
	else \
		docker-compose exec -T dev ./build/debug.asan/$(TEST_COMPONENT); \
	fi

test-async.asan: build/debug.asan/$(TEST_ASYNC)
	@echo "Running async delegation tests with ASan..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug.asan/$(TEST_ASYNC); \
	else \
		docker-compose exec -T dev ./build/debug.asan/$(TEST_ASYNC); \
	fi

test-distributed.asan: build/debug.asan/$(TEST_DISTRIBUTED)
	@echo "Running distributed hierarchy tests with ASan..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug.asan/$(TEST_DISTRIBUTED); \
	else \
		docker-compose exec -T dev ./build/debug.asan/$(TEST_DISTRIBUTED); \
	fi

test-unit.asan: build/debug.asan/$(TEST_UNIT)
	@echo "Running unit tests with ASan..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug.asan/$(TEST_UNIT); \
	else \
		docker-compose exec -T dev ./build/debug.asan/$(TEST_UNIT); \
	fi

test-concurrency.asan: build/debug.asan/$(TEST_CONCURRENCY)
	@echo "Running concurrency unit tests with ASan..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug.asan/$(TEST_CONCURRENCY); \
	else \
		docker-compose exec -T dev ./build/debug.asan/$(TEST_CONCURRENCY); \
	fi

test-simulation.asan: build/debug.asan/$(TEST_SIMULATION)
	@echo "Running simulation unit tests with ASan..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/debug.asan/$(TEST_SIMULATION); \
	else \
		docker-compose exec -T dev ./build/debug.asan/$(TEST_SIMULATION); \
	fi

test-grpc.asan: build/release.asan/$(TEST_GRPC)
	@echo "Running gRPC distributed tests with ASan..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/release.asan/$(TEST_GRPC); \
	else \
		docker-compose exec -T dev ./build/release.asan/$(TEST_GRPC); \
	fi

# Run all tests for debug.asan build
test-all.asan: build/debug.asan/$(TEST_BASIC)
	@echo "Running all tests for debug.asan mode..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		cd build/debug.asan && ctest --output-on-failure -j$(NPROC); \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev bash -c "cd build/debug.asan && ctest --output-on-failure -j$(NPROC)"; \
	fi

# Test with GTest filter
.PHONY: test-filter
test-filter:
	@echo "Usage: make test-filter BUILD=<build> SUITE=<suite> FILTER=<filter>"
	@echo "Example: make test-filter BUILD=debug SUITE=basic_delegation_tests FILTER='E2E_Phase1.*'"

test-filter-%:
	@echo "Running $(SUITE) with filter: $(FILTER) on $(BUILD) build"
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/$(BUILD)/$(SUITE) --gtest_filter="$(FILTER)"; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev ./build/$(BUILD)/$(SUITE) --gtest_filter="$(FILTER)"; \
	fi

# ============================================================================
# Load Testing & Benchmarks
# ============================================================================

.PHONY: load-test load-test-quick load-test-scenario benchmark benchmark-message-pool benchmark-distributed benchmark-strings

load-test: build/release/$(TEST_BASIC)
	@echo "Running load tests (full duration)..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./tests/load/run_all_scenarios.sh; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev ./tests/load/run_all_scenarios.sh; \
	fi

load-test-quick: build/release/$(TEST_BASIC)
	@echo "Running load tests (quick mode)..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./tests/load/run_all_scenarios.sh --quick; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev ./tests/load/run_all_scenarios.sh --quick; \
	fi

load-test-scenario:
	@echo "Usage: make load-test-scenario SCENARIO=<scenario>"
	@echo "Example: make load-test-scenario SCENARIO=high_load"

load-test-scenario-%: build/release/$(TEST_BASIC)
	@echo "Running load test scenario: $*..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/release/hmas_load_test --scenario=$* --duration=300; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev ./build/release/hmas_load_test --scenario=$* --duration=300; \
	fi

benchmark: build/release/$(TEST_BASIC)
	@echo "Running benchmarks..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./scripts/run_benchmarks.sh; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev ./scripts/run_benchmarks.sh; \
	fi

benchmark-message-pool: build/release/$(TEST_BASIC)
	@echo "Running message pool benchmarks..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/release/message_pool_benchmarks; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev ./build/release/message_pool_benchmarks; \
	fi

benchmark-distributed: build/release/$(TEST_BASIC)
	@echo "Running distributed benchmarks..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/release/distributed_benchmarks; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev ./build/release/distributed_benchmarks; \
	fi

benchmark-strings: build/release/$(TEST_BASIC)
	@echo "Running string allocation benchmarks..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./build/release/string_allocation_benchmarks; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev ./build/release/string_allocation_benchmarks; \
	fi

# ============================================================================
# Linting & Static Analysis
# ============================================================================

.PHONY: lint lint-clang-tidy lint-cppcheck

lint:
	@echo "Running static analysis..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./scripts/run_static_analysis.sh; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev ./scripts/run_static_analysis.sh; \
	fi

lint-clang-tidy:
	@echo "Running clang-tidy..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./scripts/run_static_analysis.sh --clang-tidy-only; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev ./scripts/run_static_analysis.sh --clang-tidy-only; \
	fi

lint-cppcheck:
	@echo "Running cppcheck..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		./scripts/run_static_analysis.sh --cppcheck-only; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev ./scripts/run_static_analysis.sh --cppcheck-only; \
	fi

# ============================================================================
# Code Formatting
# ============================================================================

.PHONY: format format-check

format:
	@echo "Formatting C++ code with clang-format..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		find src include tests benchmarks -type f \( -name "*.cpp" -o -name "*.hpp" \) \
			-not -path "*/build/*" -not -path "*/_deps/*" \
			| xargs clang-format -i; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev bash -c \
			"find src include tests benchmarks -type f \( -name '*.cpp' -o -name '*.hpp' \) \
			-not -path '*/build/*' -not -path '*/_deps/*' \
			| xargs clang-format -i"; \
	fi
	@echo "✓ Formatting complete"

format-check:
	@echo "Checking C++ formatting..."
	@if [ "$(NATIVE_MODE)" = "true" ]; then \
		find src include tests benchmarks -type f \( -name "*.cpp" -o -name "*.hpp" \) \
			-not -path "*/build/*" -not -path "*/_deps/*" \
			| xargs clang-format --dry-run --Werror; \
	else \
		$(MAKE) -C docker ensure-running 2>/dev/null || true; \
		docker-compose exec -T dev bash -c \
			"find src include tests benchmarks -type f \( -name '*.cpp' -o -name '*.hpp' \) \
			-not -path '*/build/*' -not -path '*/_deps/*' \
			| xargs clang-format --dry-run --Werror"; \
	fi
	@echo "✓ Formatting check passed"

# ============================================================================
# Clean Recipes
# ============================================================================

.PHONY: clean clean-all clean-everything

# Clean specific build directory
clean:
	@echo "Usage: make clean BUILD=<build>"
	@echo "Example: make clean BUILD=debug"

clean-debug:
	@echo "Cleaning build/debug..."
	rm -rf build/debug

clean-release:
	@echo "Cleaning build/release..."
	rm -rf build/release

clean-debug.asan:
	@echo "Cleaning build/debug.asan..."
	rm -rf build/debug.asan

clean-release.asan:
	@echo "Cleaning build/release.asan..."
	rm -rf build/release.asan

# Clean all build directories
clean-all:
	@echo "Cleaning all build directories..."
	rm -rf build/debug build/release build/debug.asan build/release.asan

# Clean build artifacts and Docker resources
clean-everything: clean-all docker-clean
	@echo "Cleaned everything"

# ============================================================================
# Docker Management
# ============================================================================

.PHONY: docker-build docker-up docker-clean docker-down docker-shell

docker-build:
	@echo "Building Docker image: dev (commit $(GIT_COMMIT))..."
	docker-compose build dev

docker-build-%:
	@echo "Building Docker image: $* (commit $(GIT_COMMIT))..."
	docker-compose build $*

docker-up:
	@echo "Starting dev container for commit $(GIT_COMMIT)..."
	docker-compose up -d dev
	sleep 2

docker-clean:
	@echo "Cleaning Docker resources..."
	docker-compose down -v
	docker rmi -f projectkeystone-dev:$(GIT_COMMIT) projectkeystone:$(GIT_COMMIT) || true

docker-down:
	@echo "Stopping containers..."
	docker-compose down

docker-shell: docker-up
	docker-compose exec dev /bin/bash

# ============================================================================
# Native Variants (run on host system instead of Docker)
# ============================================================================

.PHONY: debug.native release.native debug.asan.native release.asan.native test.native test-basic.native test-module.native test-component.native test-async.native test-unit.native test-all.native test.asan.native test-basic.asan.native test-module.asan.native test-component.asan.native test-async.asan.native test-unit.asan.native test-all.asan.native lint.native lint-clang-tidy.native lint-cppcheck.native format.native format-check.native benchmark.native load-test.native load-test-quick.native ci.native ci-quick.native pre-commit.native

# Native build aliases
debug.native: NATIVE=1
debug.native: debug

release.native: NATIVE=1
release.native: release

debug.asan.native: NATIVE=1
debug.asan.native: debug.asan

release.asan.native: NATIVE=1
release.asan.native: release.asan

# Native test aliases
test.native: NATIVE=1
test.native: test

test-basic.native: NATIVE=1
test-basic.native: test-basic

test-module.native: NATIVE=1
test-module.native: test-module

test-component.native: NATIVE=1
test-component.native: test-component

test-async.native: NATIVE=1
test-async.native: test-async

test-unit.native: NATIVE=1
test-unit.native: test-unit

test-all.native: NATIVE=1
test-all.native: test-all

test.asan.native: NATIVE=1
test.asan.native: test.asan

test-basic.asan.native: NATIVE=1
test-basic.asan.native: test-basic.asan

test-module.asan.native: NATIVE=1
test-module.asan.native: test-module.asan

test-component.asan.native: NATIVE=1
test-component.asan.native: test-component.asan

test-async.asan.native: NATIVE=1
test-async.asan.native: test-async.asan

test-unit.asan.native: NATIVE=1
test-unit.asan.native: test-unit.asan

test-all.asan.native: NATIVE=1
test-all.asan.native: test-all.asan

# Native lint aliases
lint.native: NATIVE=1
lint.native: lint

lint-clang-tidy.native: NATIVE=1
lint-clang-tidy.native: lint-clang-tidy

lint-cppcheck.native: NATIVE=1
lint-cppcheck.native: lint-cppcheck

format.native: NATIVE=1
format.native: format

format-check.native: NATIVE=1
format-check.native: format-check

# Native benchmark aliases
benchmark.native: NATIVE=1
benchmark.native: benchmark

load-test.native: NATIVE=1
load-test.native: load-test

load-test-quick.native: NATIVE=1
load-test-quick.native: load-test-quick

# Native CI/CD aliases
ci.native: NATIVE=1
ci.native: ci

ci-quick.native: NATIVE=1
ci-quick.native: ci-quick

pre-commit.native: NATIVE=1
pre-commit.native: pre-commit

# ============================================================================
# CI/CD Helper Recipes
# ============================================================================

.PHONY: ci ci-quick pre-commit

# Full CI pipeline
ci: debug.asan test-all.asan lint format-check
	@echo "✓ CI pipeline complete"

# Quick CI (for pull requests)
ci-quick: debug.asan test-basic test-module test-component format-check
	@echo "✓ Quick CI complete"

# Pre-commit checks
pre-commit: format-check lint-clang-tidy test-basic
	@echo "✓ Pre-commit checks passed"

# ============================================================================
# Help & Info
# ============================================================================

.PHONY: help

help:
	@echo "ProjectKeystone Makefile"
	@echo "Simple build system with debug, release, and asan modes"
	@echo ""
	@echo "Usage: make <target>"
	@echo ""
	@echo "Build Commands:"
	@echo "  make                    Build debug mode (build/debug)"
	@echo "  make release            Build release mode (build/release)"
	@echo "  make debug.asan         Build debug with ASan (build/debug.asan)"
	@echo "  make release.asan       Build release with ASan (build/release.asan)"
	@echo "  make build-all          Build all modes"
	@echo ""
	@echo "Test Commands:"
	@echo "  make test               Run all tests (debug)"
	@echo "  make test-basic         Run basic delegation tests (debug)"
	@echo "  make test-module        Run module coordination tests (debug)"
	@echo "  make test-component     Run component coordination tests (debug)"
	@echo "  make test-async         Run async delegation tests (debug)"
	@echo "  make test-unit          Run unit tests (debug)"
	@echo "  make test-all           Run all tests (debug)"
	@echo ""
	@echo "  make test.asan          Run all tests with ASan (debug.asan)"
	@echo "  make test-basic.asan    Run basic delegation tests with ASan (debug.asan)"
	@echo "  make test-module.asan   Run module coordination tests with ASan (debug.asan)"
	@echo "  make test-component.asan Run component coordination tests with ASan (debug.asan)"
	@echo "  make test-async.asan    Run async delegation tests with ASan (debug.asan)"
	@echo "  make test-unit.asan     Run unit tests with ASan (debug.asan)"
	@echo "  make test-all.asan      Run all tests with ASan (debug.asan)"
	@echo ""
	@echo "  make test-filter BUILD=<build> SUITE=<suite> FILTER=<filter>"
	@echo "                          Run test with GTest filter"
	@echo ""
	@echo "Load & Benchmark:"
	@echo "  make load-test          Run all load tests"
	@echo "  make load-test-quick    Run quick load tests (CI)"
	@echo "  make load-test-scenario SCENARIO=<scenario>  Run specific load test scenario"
	@echo "  make benchmark          Run all benchmarks"
	@echo "  make benchmark-message-pool  Run message pool benchmarks"
	@echo "  make benchmark-distributed Run distributed benchmarks"
	@echo "  make benchmark-strings  Run string allocation benchmarks"
	@echo ""
	@echo "Lint & Format:"
	@echo "  make lint               Run all linters"
	@echo "  make lint-clang-tidy    Run clang-tidy only"
	@echo "  make lint-cppcheck      Run cppcheck only"
	@echo "  make format             Format all C++ files"
	@echo "  make format-check       Check formatting (CI)"
	@echo ""
	@echo "Docker:"
	@echo "  make docker-build       Build Docker image"
	@echo "  make docker-up          Start dev container"
	@echo "  make docker-down        Stop containers"
	@echo "  make docker-shell       Enter dev container"
	@echo ""
	@echo "Clean:"
	@echo "  make clean BUILD=<build>  Clean specific build dir"
	@echo "  make clean-all          Clean all build dirs"
	@echo ""
	@echo "Native Mode:"
	@echo "  Add 'native-' prefix or set NATIVE=1"
	@echo "  Example: make native-debug"
	@echo "           NATIVE=1 make debug"
	@echo ""
	@echo "CI/CD:"
	@echo "  make ci                 Full CI pipeline"
	@echo "  make ci-quick           Quick CI for PRs"
	@echo "  make pre-commit         Pre-commit checks"