cmake_minimum_required(VERSION 3.20)
project(ProjectKeystone CXX)

# C++20 standard required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
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
        URL https://github.com/google/googletest/archive/release-1.12.1.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# concurrentqueue - Lock-free MPMC queue
FetchContent_Declare(
    concurrentqueue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG v1.0.4
)

# Cista - Zero-copy serialization
FetchContent_Declare(
    cista
    GIT_REPOSITORY https://github.com/felixguendling/cista.git
    GIT_TAG v0.14
)

# spdlog - Fast logging library
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)

FetchContent_MakeAvailable(concurrentqueue cista spdlog)

enable_testing()

# Core library
add_library(keystone_core
    src/core/message.cpp
    src/core/message_bus.cpp
    src/core/message_serializer.cpp
)

target_include_directories(keystone_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${cista_SOURCE_DIR}/include
)

# Concurrency library (Phase A)
add_library(keystone_concurrency
    src/concurrency/task.cpp
    src/concurrency/thread_pool.cpp
    src/concurrency/work_stealing_queue.cpp
    src/concurrency/logger.cpp
)

target_include_directories(keystone_concurrency PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${concurrentqueue_SOURCE_DIR}
)

target_link_libraries(keystone_concurrency PUBLIC
    keystone_core
    spdlog::spdlog
)

# Agents library
add_library(keystone_agents
    src/agents/base_agent.cpp
    src/agents/chief_architect_agent.cpp
    src/agents/component_lead_agent.cpp
    src/agents/module_lead_agent.cpp
    src/agents/task_agent.cpp
)

target_link_libraries(keystone_agents PUBLIC
    keystone_core
    keystone_concurrency
)

# E2E Tests - Phase 1
add_executable(phase1_e2e_tests
    tests/e2e/phase1_basic_delegation.cpp
)

target_link_libraries(phase1_e2e_tests
    keystone_agents
    keystone_core
    GTest::gtest_main
)

gtest_discover_tests(phase1_e2e_tests)

# E2E Tests - Phase 2
add_executable(phase2_e2e_tests
    tests/e2e/phase2_module_coordination.cpp
)

target_link_libraries(phase2_e2e_tests
    keystone_agents
    keystone_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(phase2_e2e_tests)

# E2E Tests - Phase 3
add_executable(phase3_e2e_tests
    tests/e2e/phase3_component_coordination.cpp
)

target_link_libraries(phase3_e2e_tests
    keystone_agents
    keystone_core
    GTest::gtest_main
)

gtest_discover_tests(phase3_e2e_tests)

# Unit Tests
add_executable(unit_tests
    tests/unit/test_message_bus.cpp
    tests/unit/test_message_serializer.cpp
)

target_link_libraries(unit_tests
    keystone_agents
    keystone_core
    GTest::gtest_main
)

gtest_discover_tests(unit_tests)

# Unit Tests - Concurrency (Phase A)
add_executable(concurrency_unit_tests
    tests/unit/test_task.cpp
    tests/unit/test_thread_pool.cpp
    tests/unit/test_work_stealing_queue.cpp
    tests/unit/test_logger.cpp
)

target_link_libraries(concurrency_unit_tests
    keystone_concurrency
    keystone_core
    GTest::gtest_main
)

gtest_discover_tests(concurrency_unit_tests)

# Add test target
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS phase1_e2e_tests phase2_e2e_tests phase3_e2e_tests unit_tests concurrency_unit_tests
)
