# ProjectKeystone Build System

## Overview

ProjectKeystone uses CMake 3.28+ with C++20 module support to manage compilation, dependencies, and multi-platform builds. This document outlines the build configuration, toolchain requirements, and development workflows.

## Toolchain Requirements

### Compiler Support

| Compiler | Minimum Version | C++20 Modules | Coroutines | Recommended |
|----------|----------------|---------------|------------|-------------|
| **GCC** | 13.0 | ✅ Full | ✅ Full | 13.2+ |
| **Clang** | 16.0 | ✅ Full | ✅ Full | 17.0+ |
| **MSVC** | 17.5 (VS 2022) | ✅ Full | ✅ Full | 17.8+ |

### Build Tools

- **CMake**: 3.28 or later (required for module support)
- **Ninja**: 1.11+ (recommended build system)
- **Make**: 4.3+ (alternative to Ninja)

### Package Managers

**Primary Choice: vcpkg** (recommended for C++20 module compatibility)

```bash
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # Linux/macOS
# or
./bootstrap-vcpkg.bat  # Windows
```

**Alternative: Conan** (v2.0+)

## Project Structure

```
ProjectKeystone/
├── CMakeLists.txt              # Root CMake configuration
├── CMakePresets.json           # Build presets
├── vcpkg.json                  # Dependency manifest
├── cmake/
│   ├── Keystone.cmake          # Custom CMake functions
│   ├── CompilerWarnings.cmake  # Warning configurations
│   └── Sanitizers.cmake        # Sanitizer configurations
├── modules/
│   ├── Keystone.Core/
│   │   ├── CMakeLists.txt
│   │   ├── Core.cppm           # Module interface
│   │   └── impl/               # Implementation files
│   ├── Keystone.Protocol/
│   ├── Keystone.Agents/
│   └── Keystone.Integration/
├── tests/
│   ├── CMakeLists.txt
│   ├── unit/
│   ├── integration/
│   └── performance/
├── examples/
│   └── CMakeLists.txt
└── third_party/                # Git submodules if needed
```

## Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.28)

project(ProjectKeystone
    VERSION 1.0.0
    DESCRIPTION "High-Performance Hierarchical Multi-Agent System"
    LANGUAGES CXX
)

# C++20 Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable C++20 Modules (experimental)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "aa1f7df0-828a-4fcd-9afc-2dc80491aca7")
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP ON)

# Build options
option(KEYSTONE_BUILD_TESTS "Build tests" ON)
option(KEYSTONE_BUILD_EXAMPLES "Build examples" ON)
option(KEYSTONE_BUILD_BENCHMARKS "Build benchmarks" ON)
option(KEYSTONE_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(KEYSTONE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(KEYSTONE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(KEYSTONE_ENABLE_LTO "Enable Link-Time Optimization" OFF)

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Keystone)
include(CompilerWarnings)
include(Sanitizers)

# Find dependencies
find_package(concurrentqueue CONFIG REQUIRED)
find_package(cista CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Optional dependencies
if(KEYSTONE_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
endif()

if(KEYSTONE_BUILD_BENCHMARKS)
    find_package(benchmark CONFIG REQUIRED)
endif()

# Add subdirectories
add_subdirectory(modules/Keystone.Core)
add_subdirectory(modules/Keystone.Protocol)
add_subdirectory(modules/Keystone.Agents)
add_subdirectory(modules/Keystone.Integration)

if(KEYSTONE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(KEYSTONE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation rules
include(GNUInstallDirs)
install(TARGETS Keystone.Core Keystone.Protocol Keystone.Agents Keystone.Integration
    EXPORT KeystoneTargets
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(EXPORT KeystoneTargets
    FILE KeystoneTargets.cmake
    NAMESPACE Keystone::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Keystone
)
```

## CMake Presets (CMakePresets.json)

```json
{
    "version": 6,
    "cmakeMinimumRequired": {
        "major": 3,
        "minor": 28,
        "patch": 0
    },
    "configurePresets": [
        {
            "name": "default",
            "hidden": true,
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/${presetName}",
            "cacheVariables": {
                "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
            }
        },
        {
            "name": "debug",
            "inherits": "default",
            "displayName": "Debug Build",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "KEYSTONE_ENABLE_ASAN": "ON"
            }
        },
        {
            "name": "release",
            "inherits": "default",
            "displayName": "Release Build",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release",
                "KEYSTONE_ENABLE_LTO": "ON"
            }
        },
        {
            "name": "relwithdebinfo",
            "inherits": "default",
            "displayName": "Release with Debug Info",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "RelWithDebInfo"
            }
        },
        {
            "name": "tsan",
            "inherits": "default",
            "displayName": "ThreadSanitizer Build",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "KEYSTONE_ENABLE_TSAN": "ON"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "debug",
            "configurePreset": "debug"
        },
        {
            "name": "release",
            "configurePreset": "release"
        }
    ],
    "testPresets": [
        {
            "name": "default",
            "configurePreset": "debug",
            "output": {
                "outputOnFailure": true
            }
        }
    ]
}
```

## Module CMakeLists.txt Example

### Keystone.Core Module

```cmake
# modules/Keystone.Core/CMakeLists.txt

add_library(Keystone.Core)
add_library(Keystone::Core ALIAS Keystone.Core)

# Module interface files
target_sources(Keystone.Core
    PUBLIC
        FILE_SET CXX_MODULES FILES
            Core.cppm
            Concurrency.cppm
            Messaging.cppm
            Synchronization.cppm
            Utilities.cppm
)

# Implementation files
target_sources(Keystone.Core
    PRIVATE
        impl/ThreadPool.cpp
        impl/MessageQueue.cpp
        impl/Synchronization.cpp
        impl/Logger.cpp
        impl/Metrics.cpp
)

# Include directories for implementation files
target_include_directories(Keystone.Core
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/impl
)

# Link dependencies
target_link_libraries(Keystone.Core
    PUBLIC
        concurrentqueue::concurrentqueue
        spdlog::spdlog
    PRIVATE
        Threads::Threads
)

# Compiler features
target_compile_features(Keystone.Core PUBLIC cxx_std_20)

# Warnings
keystone_set_warnings(Keystone.Core)

# Sanitizers
keystone_enable_sanitizers(Keystone.Core)
```

## Dependency Management (vcpkg.json)

```json
{
    "name": "project-keystone",
    "version": "1.0.0",
    "dependencies": [
        {
            "name": "concurrentqueue",
            "version>=": "1.0.4"
        },
        {
            "name": "cista",
            "version>=": "0.14"
        },
        {
            "name": "grpc",
            "version>=": "1.54.0"
        },
        {
            "name": "protobuf",
            "version>=": "3.21.0"
        },
        {
            "name": "spdlog",
            "version>=": "1.12.0",
            "features": ["fmt-external"]
        },
        {
            "name": "onnxruntime",
            "version>=": "1.15.0",
            "platform": "!uwp"
        },
        {
            "name": "prometheus-cpp",
            "version>=": "1.1.0"
        }
    ],
    "builtin-baseline": "TBD",
    "overrides": []
}
```

## Custom CMake Modules

### cmake/Keystone.cmake

```cmake
# Helper functions for Keystone project

function(keystone_add_module MODULE_NAME)
    add_library(${MODULE_NAME})
    add_library(Keystone::${MODULE_NAME} ALIAS ${MODULE_NAME})

    target_compile_features(${MODULE_NAME} PUBLIC cxx_std_20)

    # Apply common settings
    keystone_set_warnings(${MODULE_NAME})

    if(KEYSTONE_ENABLE_LTO)
        set_property(TARGET ${MODULE_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endfunction()
```

### cmake/CompilerWarnings.cmake

```cmake
function(keystone_set_warnings TARGET_NAME)
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE
            /W4          # High warning level
            /WX          # Treat warnings as errors
            /permissive- # Strict standards compliance
        )
    else()
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror      # Treat warnings as errors
            -Wconversion
            -Wsign-conversion
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
        )

        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${TARGET_NAME} PRIVATE
                -Wmost
                -Wthread-safety
            )
        endif()
    endif()
endfunction()
```

### cmake/Sanitizers.cmake

```cmake
function(keystone_enable_sanitizers TARGET_NAME)
    if(KEYSTONE_ENABLE_ASAN)
        target_compile_options(${TARGET_NAME} PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer
        )
        target_link_options(${TARGET_NAME} PRIVATE -fsanitize=address)
    endif()

    if(KEYSTONE_ENABLE_TSAN)
        target_compile_options(${TARGET_NAME} PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
        )
        target_link_options(${TARGET_NAME} PRIVATE -fsanitize=thread)
    endif()

    if(KEYSTONE_ENABLE_UBSAN)
        target_compile_options(${TARGET_NAME} PRIVATE
            -fsanitize=undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${TARGET_NAME} PRIVATE -fsanitize=undefined)
    endif()
endfunction()
```

## Build Workflows

### Local Development Build

```bash
# Configure with debug preset
cmake --preset debug

# Build
cmake --build build/debug --parallel

# Run tests
ctest --preset default
```

### Release Build

```bash
# Configure
cmake --preset release

# Build with optimizations
cmake --build build/release --parallel

# Install
cmake --install build/release --prefix /usr/local
```

### With ThreadSanitizer

```bash
# Configure
cmake --preset tsan

# Build
cmake --build build/tsan --parallel

# Run tests (will detect data races)
cd build/tsan
ctest --output-on-failure
```

## CI/CD Integration

### GitHub Actions Workflow

```yaml
# .github/workflows/build.yml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-22.04, windows-2022, macos-13]
        compiler:
          - { name: gcc, version: 13 }
          - { name: clang, version: 17 }
          - { name: msvc, version: latest }
        exclude:
          - os: windows-2022
            compiler: { name: gcc }
          - os: ubuntu-22.04
            compiler: { name: msvc }
          - os: macos-13
            compiler: { name: msvc }

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Install vcpkg
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: 'latest'

      - name: Configure CMake
        run: cmake --preset debug

      - name: Build
        run: cmake --build build/debug --parallel

      - name: Test
        run: ctest --preset default

      - name: Upload coverage
        if: matrix.os == 'ubuntu-22.04'
        uses: codecov/codecov-action@v3
```

## Platform-Specific Considerations

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    gcc-13 \
    g++-13 \
    git

# Set default compiler
export CC=gcc-13
export CXX=g++-13
```

### macOS

```bash
# Install via Homebrew
brew install cmake ninja llvm@17

# Use LLVM toolchain
export CC=/usr/local/opt/llvm@17/bin/clang
export CXX=/usr/local/opt/llvm@17/bin/clang++
```

### Windows

```powershell
# Install Visual Studio 2022 with C++ workload
# Install CMake and Ninja via Visual Studio installer

# Use Developer Command Prompt
cmake --preset debug
cmake --build build/debug
```

## Performance Optimization Flags

### GCC/Clang Release Flags

```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${TARGET_NAME} PRIVATE
        -O3                    # Maximum optimization
        -march=native          # CPU-specific optimizations
        -flto                  # Link-time optimization
        -ffast-math            # Fast floating-point math
        -funroll-loops         # Loop unrolling
    )
endif()
```

### MSVC Release Flags

```cmake
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${TARGET_NAME} PRIVATE
        /O2                    # Maximum optimization
        /GL                    # Whole program optimization
        /arch:AVX2             # AVX2 instructions
    )
    target_link_options(${TARGET_NAME} PRIVATE
        /LTCG                  # Link-time code generation
    )
endif()
```

## Troubleshooting

### Module Build Issues

**Problem**: "C++20 modules not supported"
**Solution**: Upgrade CMake to 3.28+ and compiler to minimum version

**Problem**: Module dependency errors
**Solution**: Clean build directory: `rm -rf build && cmake --preset debug`

**Problem**: Slow module compilation
**Solution**: Use Ninja instead of Make: `-G Ninja`

### Dependency Issues

**Problem**: vcpkg packages not found
**Solution**: Specify vcpkg toolchain: `-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake`

**Problem**: Version conflicts
**Solution**: Update `vcpkg.json` baseline and run `vcpkg update`

---

**Document Version**: 1.0
**Last Updated**: 2025-11-17
