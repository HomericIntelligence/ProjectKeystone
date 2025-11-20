/**
 * @file test_task.cpp
 * @brief Unit tests for Task<T> coroutine type
 */

#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <string>

#include "concurrency/task.hpp"

using namespace keystone::concurrency;

// Test: Simple Task<int> creation and get()
TEST(TaskTest, SimpleIntTask) {
  auto task = []() -> Task<int> { co_return 42; }();

  EXPECT_FALSE(task.done());
  int result = task.get();
  EXPECT_EQ(result, 42);
  EXPECT_TRUE(task.done());
}

// Test: Simple Task<void> creation and get()
TEST(TaskTest, SimpleVoidTask) {
  auto executed = std::make_shared<std::atomic<bool>>(false);

  auto lambda = [executed]() -> Task<void> {
    executed->store(true);
    co_return;
  };

  auto task = lambda();

  EXPECT_FALSE(executed->load());
  EXPECT_FALSE(task.done());

  task.get();

  EXPECT_TRUE(executed->load());
  EXPECT_TRUE(task.done());
}

// Test: Task<std::string> with move semantics
TEST(TaskTest, StringTask) {
  auto task = []() -> Task<std::string> {
    co_return "Hello from coroutine!";
  }();

  std::string result = task.get();
  EXPECT_EQ(result, "Hello from coroutine!");
}

// Test: Exception propagation
TEST(TaskTest, ExceptionPropagation) {
  auto task = []() -> Task<int> {
    throw std::runtime_error("Test exception");
    co_return 42;  // Never reached
  }();

  EXPECT_THROW({ task.get(); }, std::runtime_error);
}

// Test: Task move constructor
TEST(TaskTest, MoveConstructor) {
  auto task1 = []() -> Task<int> { co_return 100; }();

  Task<int> task2 = std::move(task1);

  int result = task2.get();
  EXPECT_EQ(result, 100);
}

// Test: Task move assignment
TEST(TaskTest, MoveAssignment) {
  auto task1 = []() -> Task<int> { co_return 200; }();

  auto task2 = []() -> Task<int> { co_return 300; }();

  task2 = std::move(task1);

  int result = task2.get();
  EXPECT_EQ(result, 200);
}

// Test: Manual resume
TEST(TaskTest, ManualResume) {
  auto task = []() -> Task<int> { co_return 42; }();

  EXPECT_FALSE(task.done());

  // Resume manually
  task.resume();

  EXPECT_TRUE(task.done());
  EXPECT_EQ(task.get(), 42);
}

// Test: Chaining coroutines with co_await
TEST(TaskTest, CoroutineChaining) {
  auto inner = []() -> Task<int> { co_return 10; };

  auto outer = [&]() -> Task<int> {
    int value = co_await inner();
    co_return value * 2;
  }();

  EXPECT_EQ(outer.get(), 20);
}

// Test: Multiple co_await in sequence
TEST(TaskTest, MultipleCoAwait) {
  auto getValue = [](int x) -> Task<int> { co_return x; };

  auto sumTask = [&]() -> Task<int> {
    int a = co_await getValue(10);
    int b = co_await getValue(20);
    int c = co_await getValue(30);
    co_return a + b + c;
  }();

  EXPECT_EQ(sumTask.get(), 60);
}

// Test: Exception in chained coroutine
TEST(TaskTest, ExceptionInChainedCoroutine) {
  auto throwingTask = []() -> Task<int> {
    throw std::runtime_error("Inner exception");
    co_return 0;
  };

  auto outerTask = [&]() -> Task<int> {
    int value = co_await throwingTask();
    co_return value;
  }();

  EXPECT_THROW({ outerTask.get(); }, std::runtime_error);
}

// Test: Task<void> chaining
TEST(TaskTest, VoidTaskChaining) {
  auto counter = std::make_shared<std::atomic<int>>(0);

  auto increment = [counter]() -> Task<void> {
    counter->fetch_add(1);
    co_return;
  };

  auto multiIncrementLambda = [&increment]() -> Task<void> {
    co_await increment();
    co_await increment();
    co_await increment();
    co_return;
  };

  auto multiIncrement = multiIncrementLambda();

  EXPECT_EQ(counter->load(), 0);
  multiIncrement.get();
  EXPECT_EQ(counter->load(), 3);
}

// Test: await_ready returns correct value
TEST(TaskTest, AwaitReady) {
  auto task = []() -> Task<int> { co_return 42; }();

  // Before resume, not ready
  EXPECT_FALSE(task.await_ready());

  // After resume, ready
  task.resume();
  EXPECT_TRUE(task.await_ready());
}

// Test: Complex computation with multiple steps
TEST(TaskTest, ComplexComputation) {
  auto fibonacci = [](int n) -> Task<int> {
    if (n <= 1) {
      co_return n;
    }
    // Simplified fibonacci (not true recursive coroutines)
    co_return n;  // Placeholder
  };

  auto computeTask = [&]() -> Task<int> {
    int a = co_await fibonacci(5);
    int b = co_await fibonacci(10);
    co_return a + b;
  }();

  int result = computeTask.get();
  EXPECT_EQ(result, 15);
}

// Test: Task destruction before completion
TEST(TaskTest, EarlyDestruction) {
  // This test verifies that destroying a Task before completion is safe
  {
    auto task = []() -> Task<int> { co_return 42; }();

    EXPECT_FALSE(task.done());
    // Task destroyed here without calling get()
  }
  // If this doesn't crash, the test passes
  SUCCEED();
}

// Test: Multiple get() calls return same result
TEST(TaskTest, MultipleGetCalls) {
  auto task = []() -> Task<int> { co_return 42; }();

  int result1 = task.get();
  int result2 = task.get();

  EXPECT_EQ(result1, 42);
  EXPECT_EQ(result2, 42);
}
