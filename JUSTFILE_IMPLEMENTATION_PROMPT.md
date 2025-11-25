# Justfile Implementation Prompt Template

## Objective

Implement a comprehensive `justfile` build system that provides unified commands for building, testing, and linting the project. The justfile should support both Docker (default for CI/CD) and native execution modes with parallel build support across multiple build modes.

## Requirements Summary

1. **Docker by default** - All commands run in Docker unless explicitly using native mode
2. **Native support** - Ability to run on host system with `NATIVE=1` or `native-*` prefix
3. **Parallel builds** - Build directories: `build/<build_mode>/` (e.g., `build/debug/`, `build/release/`)
4. **Individual commands** - Separate commands for each build mode, test suite, and tool
5. **Comprehensive testing** - Support for individual test suites, all tests, and benchmarks
6. **Linters and tools** - Support for project-specific linters, formatters, and analyzers
7. **No code duplication** - Single implementation for Docker and native modes

## Project Context (Adapt to your project)

**Language**: [Mojo/Python/Other]
**Build System**: [magic/CMake/other]
**Test Framework**: [pytest/unittest/other]
**Docker Setup**: [Existing docker-compose.yml or Dockerfile]
**Build Modes Needed**: [debug, release, test, etc.]

## Implementation Steps

### 1. Analyze Current Build System

First, explore the existing project structure:
- What build tools are used? (magic, pip, cmake, etc.)
- What test frameworks are used? (pytest, unittest, etc.)
- What linters/formatters exist? (mojo format, ruff, mypy, etc.)
- What Docker setup exists? (Dockerfile, docker-compose.yml)
- What build modes are needed? (debug, release, test, benchmark)

### 2. Design Build Directory Structure

Plan the new build directory structure to support parallel builds:

```
build/
├── debug/         # Debug build with symbols
├── release/       # Optimized release build
├── test/          # Test build with coverage
├── benchmark/     # Benchmark build (optimized + profiling)
└── [other modes]  # Project-specific modes
```

### 3. Create Justfile Structure

Design the justfile with these sections:

```justfile
# ============================================================================
# Configuration Variables
# ============================================================================
# Define reusable variables for test executables, Docker images, etc.

# ============================================================================
# Docker Management
# ============================================================================
# docker-build, docker-up, docker-down, docker-shell

# ============================================================================
# Build Recipes (per mode)
# ============================================================================
# build-debug, build-release, build-test, build (alias for default)
# Each recipe should:
# 1. Check NATIVE environment variable
# 2. If NATIVE=1, run directly on host
# 3. Otherwise, run in Docker dev container
# 4. Create build/<mode>/ directory
# 5. Configure and build

# ============================================================================
# Test Recipes
# ============================================================================
# test, test-all, test-<suite>, test-<mode>
# Individual test suites, aggregated tests, mode-specific tests

# ============================================================================
# Linter & Formatting Recipes
# ============================================================================
# lint, lint-<tool>, format, format-check
# Project-specific linters and formatters

# ============================================================================
# Benchmark & Performance Recipes
# ============================================================================
# benchmark, benchmark-<specific>, profile

# ============================================================================
# Clean Recipes
# ============================================================================
# clean <MODE>, clean-all

# ============================================================================
# Native Variants (prefix with 'native-')
# ============================================================================
# native-build-<mode>, native-test, native-lint
# All native variants call Docker recipes with NATIVE=1

# ============================================================================
# CI/CD Helper Recipes
# ============================================================================
# ci, ci-quick, pre-commit

# ============================================================================
# Help & Info
# ============================================================================
# help - comprehensive help message
```

### 4. Implement Core Pattern (No Duplication)

Use this pattern for all recipes to avoid duplication:

```justfile
# Example: Build recipe
build-debug: docker-up
    #!/usr/bin/env bash
    set -e
    echo "Building debug mode..."
    if [[ "${NATIVE:-}" == "1" ]]; then
        # Native execution
        <build command for host>
    else
        # Docker execution
        docker-compose exec -T dev bash -c "<build command>"
    fi
    echo "✓ Debug build complete: build/debug/"

# Native variant (no duplication)
native-build-debug:
    @NATIVE=1 just build-debug
```

### 5. Update Docker Configuration

Modify `docker-compose.yml` to support multiple build directories:

```yaml
# Before (if using build-cache volume):
volumes:
  - .:/workspace
  - build-cache:/workspace/build   # Remove this

# After:
volumes:
  - .:/workspace
  # Allow each build mode to create its own directory
```

### 6. Update .gitignore

Add build directories to `.gitignore`:

```gitignore
# Build directories (all modes)
build/
```

### 7. Update Documentation

Update these files:
- **README.md**: Add justfile usage section in Quick Start
- **Project documentation**: Replace manual build commands with `just` commands
- **CI/CD configs**: Update to use justfile commands

Example README section:

```markdown
## Quick Start

### Using Justfile (Recommended)

```bash
# Show all available commands
just --list
just help

# Build
just build              # Default build (debug)
just build-release      # Release build

# Test
just test               # Run all tests
just test-<suite>       # Run specific suite

# Lint
just lint               # Run all linters
just format             # Format code

# Native mode (run on host)
just native-build-debug
NATIVE=1 just build-debug
```
```

### 8. Project-Specific Adaptations

**For Mojo Projects (like ml-odyssey):**
- Use `magic run mojo` for builds
- Use `magic run pytest` or `magic run mojo test` for testing
- Use `magic run mojo format` for formatting
- Include `magic run mojo package` for packaging
- Support different environments (e.g., `magic run -e cuda mojo build`)

**For Python Projects:**
- Use `pip install -e .` for builds
- Use `pytest` or `python -m unittest` for testing
- Use `ruff`, `black`, `mypy` for linting
- Include `pip wheel` for packaging

**For C++ Projects:**
- Use CMake with different build types
- Use sanitizers (ASan, TSan, UBSan, LSan)
- Use clang-format, clang-tidy, cppcheck
- Include Google Benchmark support

### 9. Example Justfile Recipes by Language

#### Mojo Project Example

```justfile
# Build with magic
build-debug: docker-up
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        magic run mojo build src/main.mojo -o build/debug/main --debug-level full
    else
        docker-compose exec -T dev bash -c \
            "magic run mojo build src/main.mojo -o build/debug/main --debug-level full"
    fi

# Run tests with pytest
test: docker-up
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        magic run pytest tests/ -v
    else
        docker-compose exec -T dev magic run pytest tests/ -v
    fi

# Format code
format: docker-up
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        find src tests -name "*.mojo" | xargs magic run mojo format -i
    else
        docker-compose exec -T dev bash -c \
            "find src tests -name '*.mojo' | xargs magic run mojo format -i"
    fi
```

#### Python Project Example

```justfile
# Build (install in editable mode)
build: docker-up
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        pip install -e .
    else
        docker-compose exec -T dev pip install -e .
    fi

# Run tests
test: docker-up
    #!/usr/bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        pytest tests/ -v --cov=src --cov-report=html
    else
        docker-compose exec -T dev pytest tests/ -v --cov=src --cov-report=html
    fi

# Lint
lint: docker-up
    #!/usr:bin/env bash
    set -e
    if [[ "${NATIVE:-}" == "1" ]]; then
        ruff check src tests
        mypy src
    else
        docker-compose exec -T dev bash -c "ruff check src tests && mypy src"
    fi
```

### 10. Testing the Justfile

Test all recipes before committing:

```bash
# Test basic commands
just --list
just help

# Test Docker mode
just docker-up
just build
just test

# Test native mode
NATIVE=1 just build
just native-test

# Test parallel builds (if supported)
just build-debug & just build-release & wait

# Verify build outputs
ls -la build/debug/
ls -la build/release/
```

### 11. CI/CD Integration

Update CI/CD pipelines to use justfile:

```yaml
# Example GitHub Actions
jobs:
  build-and-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install just
        run: cargo install just

      - name: Build Docker image
        run: just docker-build

      - name: Run tests
        run: just test

      - name: Run linters
        run: just lint

      - name: Check formatting
        run: just format-check
```

## Implementation Checklist

- [ ] Analyze current build system and tools
- [ ] Design build directory structure
- [ ] Create justfile with all sections
- [ ] Implement Docker management recipes
- [ ] Implement build recipes for all modes
- [ ] Implement test recipes
- [ ] Implement linter/formatter recipes
- [ ] Implement benchmark recipes (if applicable)
- [ ] Implement clean recipes
- [ ] Implement native variants
- [ ] Implement CI/CD helpers
- [ ] Implement help command
- [ ] Update docker-compose.yml
- [ ] Update .gitignore
- [ ] Update README.md with justfile examples
- [ ] Update project documentation
- [ ] Update CI/CD configuration
- [ ] Test all recipes in Docker mode
- [ ] Test all recipes in native mode
- [ ] Test parallel builds
- [ ] Verify no code duplication
- [ ] Document project-specific customizations

## Benefits

After implementation, developers will have:

1. **Single Command Interface**: `just <command>` for everything
2. **No Duplication**: Docker and native modes share implementation
3. **Parallel Builds**: Each mode has its own directory
4. **Easy Discovery**: `just --list` shows all commands
5. **CI/CD Friendly**: Default Docker mode for consistency
6. **Developer Friendly**: Native mode for faster iteration
7. **Self-Documenting**: Clear help text for all commands
8. **Extensible**: Easy to add new recipes

## Example Usage After Implementation

```bash
# Quick start
just build              # Build (default mode)
just test               # Run all tests

# Parallel builds
just build-debug & just build-release & wait

# Native mode for faster iteration
just native-build-debug
just native-test

# Linting and formatting
just lint
just format

# CI/CD pipeline
just ci                 # Full CI pipeline
just ci-quick           # Quick CI for PRs
just pre-commit         # Pre-commit checks

# Docker management
just docker-build       # Build images
just docker-up          # Start dev container
just docker-shell       # Enter container
```

## Reference Implementation

For a complete reference implementation, see:
- ProjectKeystone: `/home/mvillmow/ProjectKeystone/justfile`
- 700+ lines with 60+ recipes
- Supports C++20 with CMake, Ninja, sanitizers, Docker
- Full Docker/native mode support with zero duplication

## Common Pitfalls to Avoid

1. **Don't hardcode paths** - Use relative paths and environment variables
2. **Don't duplicate logic** - Always use the NATIVE pattern
3. **Don't skip docker-up dependency** - Ensure container is running
4. **Don't forget exit codes** - Use `set -e` in bash scripts
5. **Don't ignore parallelization** - Design for parallel builds from the start
6. **Don't skip documentation** - Update all docs with justfile commands
7. **Don't forget native variants** - Create `native-*` recipes for all commands

## Conclusion

This template provides a comprehensive approach to implementing a justfile-based build system that supports both Docker and native execution without code duplication. Adapt the language-specific examples to match your project's needs, and follow the implementation checklist to ensure nothing is missed.

The result will be a unified, easy-to-use interface that works consistently across development, CI/CD, and production environments.
