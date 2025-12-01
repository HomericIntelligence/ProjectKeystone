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
    DOCKER_CHECK :=
    DOCKER_PREFIX :=
else
    DOCKER_CHECK := $(MAKE) -C docker ensure-running 2>/dev/null || true;
    DOCKER_PREFIX := docker-compose exec -T dev 
endif

# Compiler flags
BUILD_FLAGS_debug := -O0 -g -D_DEBUG
BUILD_FLAGS_release := -O3 -DNDEBUG
BUILD_FLAGS_asan := -fsanitize=address -fno-omit-frame-pointer
BUILD_FLAGS_ubsan := -fsanitize=undefined -fno-omit-frame-pointer
BUILD_FLAGS_lsan := -fsanitize=leak -fno-omit-frame-pointer
BUILD_FLAGS_tsan := -fsanitize=thread -fno-omit-frame-pointer
BUILD_FLAGS_msan := -fsanitize=memory -fno-omit-frame-pointer

BUILD_DIR ?= build
BUILD_TYPE ?= Debug

# ============================================================================
# Default target
# ============================================================================

.PHONY: default
default: compile

# Directory creation rule - only runs if directory doesn't exist
.PHONY: $(BUILD_DIR)

$(BUILD_DIR):
	@echo "Creating build directory: $@"
	@mkdir -p $(BUILD_DIR)

# Generic build rule for any mode
compile: $(BUILD_DIR)
	@echo "Building $* mode..."
	$(DOCKER_CHECK)
	$(DOCKER_PREFIX) bash -c "cmake -S . -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_CXX_FLAGS=\"$(BUILD_FLAGS)\""
	$(DOCKER_PREFIX) bash -c "cmake --build $(BUILD_DIR) -j$(NPROC)"

# ============================================================================
# Linting & Static Analysis
# ============================================================================

.PHONY: lint lint-clang-tidy lint-cppcheck

lint:
	@echo "Running static analysis..."
	$(DOCKER_CHECK)
	$(DOCKER_PREFIX) ./scripts/run_static_analysis.sh;

lint-clang-tidy:
	@echo "Running clang-tidy..."
	$(DOCKER_CHECK)
	$(DOCKER_PREFIX) ./scripts/run_static_analysis.sh --clang-tidy-only;

lint-cppcheck:
	@echo "Running cppcheck..."
	$(DOCKER_CHECK)
	$(DOCKER_PREFIX) ./scripts/run_static_analysis.sh --cppcheck-only;

# ============================================================================
# Code Formatting
# ============================================================================

.PHONY: format format-check

format:
	@echo "Formatting C++ code with clang-format..."
	$(DOCKER_CHECK)
	$(DOCKER_PREFIX) bash -c \
			"find src include tests benchmarks -type f \( -name '*.cpp' -o -name '*.hpp' \) \
			-not -path '*/build/*' -not -path '*/_deps/*' \
			| xargs clang-format -i";
	@echo "✓ Formatting complete"

format-check:
	@echo "Checking C++ formatting..."
	$(DOCKER_CHECK)
	$(DOCKER_PREFIX) bash -c \
			"find src include tests benchmarks -type f \( -name '*.cpp' -o -name '*.hpp' \) \
			-not -path '*/build/*' -not -path '*/_deps/*' \
			| xargs clang-format --dry-run --Werror"; \
	@echo "✓ Formatting check passed"

# ============================================================================
# Clean Recipes
# ============================================================================

.PHONY: clean clean-all clean-everything

# Clean specific build directory
clean:
	@echo "Cleaning directory $(BUILD_DIR)..."
	rm -rf $(BUILD_DIR)

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
	$(DOCKER_PREFIX) /bin/bash

# ============================================================================
# Native Variants (run on host system instead of Docker)
# ============================================================================

# Pattern rule for native variants - matches any target with .native suffix
%.native:
	@$(MAKE) $* NATIVE=1

# Sanitizer pattern rules - append sanitizer flags to existing targets
%.asan:
	@$(MAKE) $* BUILD_FLAGS="$(BUILD_FLAGS) $(BUILD_FLAGS_asan)" BUILD_DIR="$(BUILD_DIR)$(suffix $@)"

%.ubsan:
	@$(MAKE) $* BUILD_FLAGS="$(BUILD_FLAGS) $(BUILD_FLAGS_ubsan)" BUILD_DIR="$(BUILD_DIR)$(suffix $@)"

%.lsan:
	@$(MAKE) $* BUILD_FLAGS="$(BUILD_FLAGS) $(BUILD_FLAGS_lsan)" BUILD_DIR="$(BUILD_DIR)$(suffix $@)"

%.tsan:
	@$(MAKE) $* BUILD_FLAGS="$(BUILD_FLAGS) $(BUILD_FLAGS_tsan)" BUILD_DIR="$(BUILD_DIR)$(suffix $@)"

%.msan:
	@$(MAKE) $* BUILD_FLAGS="$(BUILD_FLAGS) $(BUILD_FLAGS_msan)" BUILD_DIR="$(BUILD_DIR)$(suffix $@)"

%.debug:
	@$(MAKE) $* BUILD_FLAGS="$(BUILD_FLAGS) $(BUILD_FLAGS_debug)" BUILD_DIR="$(BUILD_DIR)$(suffix $@)" BUILD_TYPE=Debug

%.release:
	@$(MAKE) $* BUILD_FLAGS="$(BUILD_FLAGS) $(BUILD_FLAGS_release)" BUILD_DIR="$(BUILD_DIR)$(suffix $@)" BUILD_TYPE=Release


# ============================================================================
# Help & Info
# ============================================================================

.PHONY: help

help:
	@echo "ProjectKeystone Makefile"
	@echo "Simple build system with debug, release, and sanitizer modes"
	@echo ""
	@echo "Usage: make <target>"
	@echo ""
	@echo "Build Commands:"
	@echo "  make                    Build debug mode (build/debug)"
	@echo "  make release            Build release mode (build/release)"
	@echo "  make debug.asan         Build debug with ASan (build/debug.asan)"
	@echo "  make release.asan       Build release with ASan (build/release.asan)"
	@echo ""
	@echo "Sanitizer Patterns:"
	@echo "  make debug.ubsan        Build debug with UBSan (build/debug.ubsan)"
	@echo "  make release.ubsan      Build release with UBSan (build/release.ubsan)"
	@echo "  make debug.lsan         Build debug with LSan (build/debug.lsan)"
	@echo "  make release.lsan       Build release with LSan (build/release.lsan)"
	@echo "  make debug.tsan         Build debug with TSan (build/debug.tsan)"
	@echo "  make release.tsan       Build release with TSan (build/release.tsan)"
	@echo "  make debug.msan         Build debug with MSan (build/debug.msan)"
	@echo "  make release.msan       Build release with MSan (build/release.msan)"
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
	@echo "Native Mode:"
	@echo "  Add '.native' suffix to any target to run on host system"
	@echo "  Examples:"
	@echo "    make debug.native     Build debug mode on host"
	@echo "    make test.ubsan.native Run UBSan tests on host"
	@echo "    make benchmark.tsan.native Run TSan benchmarks on host"
	@echo ""
	@echo "Clean:"
	@echo "  make clean  		 Clean all builds"
	@echo "  make clean.debug 	 Clean debug build"
	@echo "  make clean.release.tsan Clean release thread sanitizer build"

