cmake_minimum_required(VERSION 3.20)
project(ProjectKeystone CXX)

# C++20 standard required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clang-tidy and other tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Code coverage (Phase 9.1)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

if(ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Code coverage enabled")
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage -fno-inline
                        -O0)
    add_link_options(--coverage)
  else()
    message(WARNING "Code coverage only supported with GCC or Clang")
  endif()
endif()

# Static analysis (Phase 9.3)
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)

if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
  if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    # clang-tidy checks configuration
    set(CLANG_TIDY_CHECKS
        "*,-fuchsia-*,-llvm-header-guard"
        ",-google-readability-todo,-readability-magic-numbers"
        ",-cppcoreguidelines-avoid-magic-numbers"
        ",-modernize-use-trailing-return-type")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=${CLANG_TIDY_CHECKS}")
  else()
    message(WARNING "clang-tidy not found")
  endif()
endif()

# Fuzz testing (Phase 9.2)
option(ENABLE_FUZZING "Enable fuzz testing with libFuzzer" OFF)

if(ENABLE_FUZZING)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Fuzz testing enabled")
    # Store fuzzing flags for use with fuzz targets only
    set(FUZZING_FLAGS "-fsanitize=fuzzer,address,undefined"
                      "-fno-omit-frame-pointer" "-g")
    # Do NOT apply globally - only to fuzz targets to avoid breaking googletest
  else()
    message(WARNING "Fuzz testing only supported with Clang "
                    "(use -DCMAKE_CXX_COMPILER=clang++)")
  endif()
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# FetchContent for dependencies
include(FetchContent)

# Google Test
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(STATUS "GTest not found, fetching from GitHub...")
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/release-1.12.1.zip)
  set(gtest_force_shared_crt
      ON
      CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# Google Benchmark
find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
  message(STATUS "Google Benchmark not found, fetching from GitHub...")
  FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3)
  set(BENCHMARK_ENABLE_TESTING
      OFF
      CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS
      OFF
      CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(benchmark)
endif()

# concurrentqueue - Lock-free MPMC queue
FetchContent_Declare(
  concurrentqueue
  GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
  GIT_TAG v1.0.4)

# Cista - Zero-copy serialization
FetchContent_Declare(
  cista
  GIT_REPOSITORY https://github.com/felixguendling/cista.git
  GIT_TAG v0.14)

# spdlog - Fast logging library
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.12.0)

FetchContent_MakeAvailable(concurrentqueue cista spdlog)

enable_testing()

# Core library
add_library(
  keystone_core
  src/core/message.cpp
  src/core/message_bus.cpp
  src/core/message_serializer.cpp
  src/core/metrics.cpp
  src/core/profiling.cpp # Phase D.1: Performance profiling
  src/core/message_pool.cpp # Phase D.2: Message pooling
  src/core/failure_injector.cpp # Phase 5.1: Chaos engineering - agent failures
  src/core/retry_policy.cpp # Phase 5.3: Chaos engineering - message retries
  src/core/heartbeat_monitor.cpp # Phase 5.4: Chaos engineering - heartbeat
  # monitoring
  src/core/circuit_breaker.cpp # Phase 5.4: Chaos engineering - circuit breaker
  src/monitoring/prometheus_exporter.cpp # Phase 6.3: Prometheus metrics
  # exporter
)

target_include_directories(
  keystone_core
  PUBLIC ${PROJECT_SOURCE_DIR}/include ${cista_SOURCE_DIR}/include
         ${concurrentqueue_SOURCE_DIR})

target_link_libraries(keystone_core PUBLIC spdlog::spdlog)

# Concurrency library (Phase A)
add_library(
  keystone_concurrency
  src/concurrency/task.cpp src/concurrency/thread_pool.cpp
  src/concurrency/work_stealing_queue.cpp src/concurrency/logger.cpp
  src/concurrency/pull_or_steal.cpp src/concurrency/work_stealing_scheduler.cpp)

target_include_directories(
  keystone_concurrency PUBLIC ${PROJECT_SOURCE_DIR}/include
                              ${concurrentqueue_SOURCE_DIR})

target_link_libraries(keystone_concurrency PUBLIC keystone_core spdlog::spdlog)

# Agents library
add_library(keystone_agents
    src/agents/agent_base.cpp
    src/agents/base_agent.cpp
    src/agents/chief_architect_agent.cpp
    src/agents/component_lead_agent.cpp
    src/agents/module_lead_agent.cpp
    src/agents/task_agent.cpp
)

target_link_libraries(keystone_agents PUBLIC keystone_core keystone_concurrency)

# Simulation library (Phase D.3)
add_library(
  keystone_simulation
  src/simulation/simulated_numa_node.cpp src/simulation/simulated_network.cpp
  src/simulation/simulated_cluster.cpp)

target_include_directories(keystone_simulation
                           PUBLIC ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(keystone_simulation PUBLIC keystone_concurrency
                                                 keystone_core)

# E2E Tests - Basic Delegation
add_executable(basic_delegation_tests tests/e2e/basic_delegation_test.cpp)

target_link_libraries(basic_delegation_tests keystone_agents keystone_core
                      GTest::gtest_main)

gtest_discover_tests(basic_delegation_tests)

# E2E Tests - Module Coordination
add_executable(module_coordination_tests tests/e2e/module_coordination_test.cpp)

target_link_libraries(module_coordination_tests keystone_agents keystone_core
                      GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(module_coordination_tests)

# E2E Tests - Component Coordination
add_executable(component_coordination_tests
               tests/e2e/component_coordination_test.cpp)

target_link_libraries(component_coordination_tests keystone_agents
                      keystone_core GTest::gtest_main)

gtest_discover_tests(component_coordination_tests)

# E2E Tests - Async Delegation (Work-Stealing)
add_executable(async_delegation_tests tests/e2e/async_delegation_test.cpp)

target_link_libraries(async_delegation_tests keystone_agents keystone_core
                      keystone_concurrency GTest::gtest_main)

gtest_discover_tests(async_delegation_tests)

# E2E Tests - Async Agents (Coroutines)
add_executable(async_agents_tests tests/e2e/async_agents_test.cpp
                                  tests/e2e/full_async_hierarchy_test.cpp)

target_link_libraries(async_agents_tests keystone_agents keystone_core
                      keystone_concurrency GTest::gtest_main)

gtest_discover_tests(async_agents_tests)

# E2E Tests - Distributed Hierarchy
add_executable(distributed_hierarchy_tests
               tests/e2e/distributed_hierarchy_test.cpp)

target_link_libraries(distributed_hierarchy_tests keystone_simulation
                      keystone_concurrency keystone_core GTest::gtest_main)

gtest_discover_tests(distributed_hierarchy_tests)

# E2E Tests - Multi-Component Hierarchy
add_executable(multi_component_tests tests/e2e/multi_component_test.cpp)

target_link_libraries(multi_component_tests keystone_agents keystone_core
                      keystone_concurrency GTest::gtest_main)

gtest_discover_tests(multi_component_tests)

# E2E Tests - Chaos Engineering
add_executable(chaos_engineering_tests tests/e2e/chaos_engineering_test.cpp)

target_link_libraries(
  chaos_engineering_tests keystone_agents keystone_core keystone_concurrency
  keystone_simulation GTest::gtest_main)

gtest_discover_tests(chaos_engineering_tests DISCOVERY_TIMEOUT 10)

# Unit Tests
add_executable(
  unit_tests
  tests/unit/test_message_bus.cpp
  tests/unit/test_message_serializer.cpp
  tests/unit/test_message_bus_async.cpp
  tests/unit/test_async_task_agent.cpp
  tests/unit/test_message_priority.cpp
  tests/unit/test_metrics.cpp
  tests/unit/test_deadline_scheduling.cpp
  tests/unit/test_profiling.cpp # Phase D: Profiling tests
  tests/unit/test_message_pool.cpp # Phase D.2: Message pool tests
  tests/unit/test_failure_injector.cpp # Phase 5.1: Chaos engineering - agent
  # failures
  tests/unit/test_retry_policy.cpp # Phase 5.3: Chaos engineering - message
  # retries
  tests/unit/test_heartbeat_monitor.cpp # Phase 5.4: Chaos engineering -
  # heartbeat monitoring
  tests/unit/test_circuit_breaker.cpp # Phase 5.4: Chaos engineering - circuit
  # breaker
)

target_link_libraries(unit_tests keystone_agents keystone_core
                      keystone_concurrency GTest::gtest_main)

gtest_discover_tests(unit_tests)

# Unit Tests - Concurrency (Phase A)
add_executable(
  concurrency_unit_tests
  tests/unit/test_task.cpp
  tests/unit/test_thread_pool.cpp
  tests/unit/test_work_stealing_queue.cpp
  tests/unit/test_logger.cpp
  tests/unit/test_pull_or_steal.cpp
  tests/unit/test_work_stealing_scheduler.cpp
  tests/unit/test_cpu_affinity.cpp # Phase D.2: CPU affinity tests
)

target_link_libraries(concurrency_unit_tests keystone_concurrency keystone_core
                      GTest::gtest_main)

gtest_discover_tests(concurrency_unit_tests)

# Unit Tests - Simulation (Phase D.3)
add_executable(
  simulation_unit_tests
  tests/unit/test_simulated_numa_node.cpp tests/unit/test_simulated_network.cpp
  tests/unit/test_simulated_cluster.cpp
  tests/unit/test_simulation_corner_cases.cpp)

target_link_libraries(simulation_unit_tests keystone_simulation
                      keystone_concurrency keystone_core GTest::gtest_main)

gtest_discover_tests(simulation_unit_tests)

# Add test target
add_custom_target(
  run_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  DEPENDS basic_delegation_tests
          module_coordination_tests
          component_coordination_tests
          async_delegation_tests
          async_agents_tests
          distributed_hierarchy_tests
          multi_component_tests
          chaos_engineering_tests
          unit_tests
          concurrency_unit_tests
          simulation_unit_tests
  COMMENT "Running all ProjectKeystone tests with failure output")

# Benchmarks
add_executable(hierarchy_benchmarks benchmarks/hierarchy_performance.cpp)

target_link_libraries(hierarchy_benchmarks keystone_agents keystone_core
                      keystone_concurrency benchmark::benchmark)

# Phase D.2: Message pool performance benchmarks
add_executable(message_pool_benchmarks benchmarks/message_pool_performance.cpp)

target_link_libraries(message_pool_benchmarks keystone_core
                      benchmark::benchmark)

# Phase D.3: Distributed work-stealing benchmarks
add_executable(distributed_benchmarks benchmarks/distributed_work_stealing.cpp)

target_link_libraries(distributed_benchmarks keystone_simulation
                      keystone_concurrency keystone_core benchmark::benchmark)

# FIX P3-10: Removed commented-out benchmarks (message_bus_performance.cpp, resilience_performance.cpp)
# These had compilation errors and were never enabled. If needed in the future, they should be
# implemented from scratch with the current API (shared_ptr, async agents).

# Phase 9.2: Fuzz testing targets (requires -DENABLE_FUZZING=ON
# -DCMAKE_CXX_COMPILER=clang++)
if(ENABLE_FUZZING)
  # Fuzz test: Message serialization
  add_executable(fuzz_message_serialization fuzz/fuzz_message_serialization.cpp)
  target_compile_options(fuzz_message_serialization PRIVATE ${FUZZING_FLAGS})
  target_link_options(fuzz_message_serialization PRIVATE ${FUZZING_FLAGS})
  target_link_libraries(fuzz_message_serialization keystone_core)

  # Fuzz test: Message bus routing
  add_executable(fuzz_message_bus_routing fuzz/fuzz_message_bus_routing.cpp)
  target_compile_options(fuzz_message_bus_routing PRIVATE ${FUZZING_FLAGS})
  target_link_options(fuzz_message_bus_routing PRIVATE ${FUZZING_FLAGS})
  target_link_libraries(fuzz_message_bus_routing keystone_core keystone_agents)

  # Fuzz test: Work-stealing scheduler
  add_executable(fuzz_work_stealing fuzz/fuzz_work_stealing.cpp)
  target_compile_options(fuzz_work_stealing PRIVATE ${FUZZING_FLAGS})
  target_link_options(fuzz_work_stealing PRIVATE ${FUZZING_FLAGS})
  target_link_libraries(fuzz_work_stealing keystone_concurrency keystone_core)

  # Fuzz test: Retry policy
  add_executable(fuzz_retry_policy fuzz/fuzz_retry_policy.cpp)
  target_compile_options(fuzz_retry_policy PRIVATE ${FUZZING_FLAGS})
  target_link_options(fuzz_retry_policy PRIVATE ${FUZZING_FLAGS})
  target_link_libraries(fuzz_retry_policy keystone_core)

  message(STATUS "Fuzz testing targets enabled:")
  message(STATUS "  - fuzz_message_serialization")
  message(STATUS "  - fuzz_message_bus_routing")
  message(STATUS "  - fuzz_work_stealing")
  message(STATUS "  - fuzz_retry_policy")
  message(STATUS "Run with: ./<fuzz_target> -max_len=4096 -runs=1000000")
endif()
