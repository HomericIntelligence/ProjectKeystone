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

# Google Test
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "GTest not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/release-1.12.1.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

enable_testing()

# Core library
add_library(keystone_core
    src/core/message.cpp
    src/core/message_bus.cpp
)

target_include_directories(keystone_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

# Agents library
add_library(keystone_agents
    src/agents/base_agent.cpp
    src/agents/chief_architect_agent.cpp
    src/agents/task_agent.cpp
)

target_link_libraries(keystone_agents PUBLIC keystone_core)

# E2E Tests
add_executable(phase1_e2e_tests
    tests/e2e/phase1_basic_delegation.cpp
)

target_link_libraries(phase1_e2e_tests
    keystone_agents
    keystone_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(phase1_e2e_tests)

# Unit Tests
add_executable(unit_tests
    tests/unit/test_message_bus.cpp
)

target_link_libraries(unit_tests
    keystone_agents
    keystone_core
    GTest::gtest_main
)

gtest_discover_tests(unit_tests)

# Add test target
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS phase1_e2e_tests unit_tests
)
