set shell := ["bash", "-c"]

# Default: list available recipes
default:
    @just --list

# Install Conan dependencies (debug profile)
deps:
    conan install . \
        --output-folder=build/debug \
        --profile=conan/profiles/debug \
        --build=missing

# Install Conan dependencies (release profile)
deps-release:
    conan install . \
        --output-folder=build/release \
        --profile=conan/profiles/default \
        --build=missing

# Build (debug, with Conan toolchain)
build: deps
    cmake --preset debug
    cmake --build --preset debug

# Build (release)
build-release: deps-release
    cmake --preset release
    cmake --build --preset release

# Run tests
test:
    ctest --preset debug --output-on-failure

# Run linters (clang-tidy)
lint:
    @if [ -f scripts/lint.sh ]; then ./scripts/lint.sh; \
    else echo "No scripts/lint.sh found — enable ENABLE_CLANG_TIDY in CMake"; fi

# Format code (clang-format)
format:
    @if [ -f scripts/format.sh ]; then ./scripts/format.sh; \
    else find include/ src/ tests/ -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i; fi

# Check formatting without modifying files
format-check:
    @if [ -f scripts/format.sh ]; then ./scripts/format.sh --check; \
    else find include/ src/ tests/ -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror; fi

# Build with coverage instrumentation
coverage: deps
    cmake --preset coverage
    cmake --build --preset coverage

# Full CI build (release + warnings-as-errors)
ci: deps-release
    cmake --preset ci
    cmake --build --preset ci
    ctest --preset ci

# Remove all build artifacts
clean:
    rm -rf build install
