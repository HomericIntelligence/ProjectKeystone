/**
 * @file test_task.cpp
 * @brief Unit tests for Task<T> coroutine type
 */

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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

// =============================================================================
// FIX P2-08: Comprehensive exception handling tests
// =============================================================================

// Test: Exception message preservation
TEST(TaskTest, ExceptionMessagePreservation) {
  const std::string error_msg = "Detailed error message with context";

  auto task = [&error_msg]() -> Task<int> {
    throw std::runtime_error(error_msg);
    co_return 0;
  }();

  try {
    task.get();
    FAIL() << "Expected exception to be thrown";
  } catch (const std::runtime_error& e) {
    EXPECT_EQ(std::string(e.what()), error_msg);
  }
}

// Test: Task<void> exception handling
TEST(TaskTest, VoidTaskExceptionHandling) {
  auto task = []() -> Task<void> {
    throw std::logic_error("Void task exception");
    co_return;
  }();

  EXPECT_THROW({ task.get(); }, std::logic_error);
}

// Test: Different exception types
TEST(TaskTest, DifferentExceptionTypes) {
  // std::invalid_argument
  auto task1 = []() -> Task<int> {
    throw std::invalid_argument("Invalid argument");
    co_return 0;
  }();
  EXPECT_THROW({ task1.get(); }, std::invalid_argument);

  // std::out_of_range
  auto task2 = []() -> Task<int> {
    throw std::out_of_range("Out of range");
    co_return 0;
  }();
  EXPECT_THROW({ task2.get(); }, std::out_of_range);

  // Custom exception
  class CustomException : public std::exception {
   public:
    const char* what() const noexcept override { return "Custom exception"; }
  };

  auto task3 = []() -> Task<int> {
    throw CustomException();
    co_return 0;
  }();
  EXPECT_THROW({ task3.get(); }, CustomException);
}

// Test: Exception after partial execution
TEST(TaskTest, ExceptionAfterPartialExecution) {
  auto counter = std::make_shared<std::atomic<int>>(0);

  auto task = [counter]() -> Task<int> {
    counter->fetch_add(1);  // This should execute
    throw std::runtime_error("Exception after increment");
    co_return 42;  // Never reached
  }();

  EXPECT_EQ(counter->load(), 0);  // Not executed yet

  EXPECT_THROW({ task.get(); }, std::runtime_error);

  EXPECT_EQ(counter->load(), 1);  // Should have incremented before exception
}

// Test: Exception in co_await chain preserves stack
TEST(TaskTest, ExceptionInCoAwaitChain) {
  auto innerTask = []() -> Task<int> {
    throw std::runtime_error("Inner task failed");
    co_return 10;
  };

  auto middleTask = [&]() -> Task<int> {
    int val = co_await innerTask();
    co_return val * 2;  // Never reached
  };

  auto outerTask = [&]() -> Task<int> {
    int val = co_await middleTask();
    co_return val * 3;  // Never reached
  }();

  try {
    outerTask.get();
    FAIL() << "Expected exception";
  } catch (const std::runtime_error& e) {
    EXPECT_EQ(std::string(e.what()), "Inner task failed");
  }
}

// Test: Multiple exceptions - only first is captured
TEST(TaskTest, MultipleExceptionsFirstCaptured) {
  auto task = []() -> Task<int> {
    try {
      throw std::runtime_error("First exception");
    } catch (...) {
      // Caught and re-thrown
      throw std::logic_error("Second exception");
    }
    co_return 0;
  }();

  // Should propagate the last thrown exception (logic_error)
  EXPECT_THROW({ task.get(); }, std::logic_error);
}

// Test: Exception with move-only types
TEST(TaskTest, ExceptionWithMoveOnlyType) {
  auto task = []() -> Task<std::unique_ptr<int>> {
    throw std::runtime_error("Move-only exception test");
    co_return std::make_unique<int>(42);
  }();

  EXPECT_THROW({ task.get(); }, std::runtime_error);
}

// Test: Verify exception stored in promise
TEST(TaskTest, ExceptionStoredInPromise) {
  auto task = []() -> Task<int> {
    throw std::runtime_error("Stored in promise");
    co_return 0;
  }();

  // Resume to trigger exception capture
  EXPECT_FALSE(task.done());
  task.resume();
  EXPECT_TRUE(task.done());

  // Exception should be stored and re-thrown on get()
  EXPECT_THROW({ task.get(); }, std::runtime_error);
}

// =============================================================================
// Symmetric Transfer Tests (FIX P1: await_suspend)
// =============================================================================

// Test: Verify symmetric transfer chains coroutines properly
TEST(TaskTest, SymmetricTransferChaining) {
  std::vector<int> execution_order;

  auto task1 = [&]() -> Task<int> {
    execution_order.push_back(1);
    co_return 10;
  };

  auto task2 = [&]() -> Task<int> {
    execution_order.push_back(2);
    int val = co_await task1();
    execution_order.push_back(3);
    co_return val * 2;
  }();

  EXPECT_EQ(task2.get(), 20);

  // Execution order: task2 starts (2), awaits task1 (1), resumes task2 (3)
  std::vector<int> expected = {2, 1, 3};
  EXPECT_EQ(execution_order, expected);
}

// Test: Verify continuation is properly stored and resumed
TEST(TaskTest, ContinuationStorageAndResumption) {
  bool inner_executed = false;
  bool outer_resumed = false;

  auto inner = [&]() -> Task<int> {
    inner_executed = true;
    co_return 42;
  };

  auto outer = [&]() -> Task<int> {
    int result = co_await inner();
    outer_resumed = true;
    co_return result;
  }();

  EXPECT_FALSE(inner_executed);
  EXPECT_FALSE(outer_resumed);

  int result = outer.get();

  EXPECT_TRUE(inner_executed);
  EXPECT_TRUE(outer_resumed);
  EXPECT_EQ(result, 42);
}

// Test: Multiple levels of coroutine chaining
TEST(TaskTest, DeepCoroutineChaining) {
  auto level3 = []() -> Task<int> { co_return 1; };

  auto level2 = [&]() -> Task<int> {
    int val = co_await level3();
    co_return val + 10;
  };

  auto level1 = [&]() -> Task<int> {
    int val = co_await level2();
    co_return val + 100;
  }();

  EXPECT_EQ(level1.get(), 111);  // 1 + 10 + 100
}

// Test: Symmetric transfer with Task<void>
TEST(TaskTest, SymmetricTransferVoidTask) {
  std::vector<int> execution_order;

  auto voidTask = [&]() -> Task<void> {
    execution_order.push_back(1);
    co_return;
  };

  auto wrapper = [&]() -> Task<int> {
    execution_order.push_back(2);
    co_await voidTask();
    execution_order.push_back(3);
    co_return 42;
  }();

  EXPECT_EQ(wrapper.get(), 42);

  std::vector<int> expected = {2, 1, 3};
  EXPECT_EQ(execution_order, expected);
}

// Test: Verify no stack overflow with many chained coroutines
TEST(TaskTest, NoStackOverflowWithManyChainedCoroutines) {
  // Create a chain of 1000 coroutines
  const int chain_length = 1000;
  std::atomic<int> counter{0};

  std::function<Task<int>(int)> chainedTask;
  chainedTask = [&](int depth) -> Task<int> {
    counter.fetch_add(1);
    if (depth <= 0) {
      co_return 0;
    }
    int result = co_await chainedTask(depth - 1);
    co_return result + 1;
  };

  auto task = chainedTask(chain_length);
  int result = task.get();

  EXPECT_EQ(result, chain_length);
  EXPECT_EQ(counter.load(), chain_length + 1);
}

// Test: Exception propagation through symmetric transfer
TEST(TaskTest, ExceptionPropagationThroughSymmetricTransfer) {
  auto throwingTask = []() -> Task<int> {
    throw std::runtime_error("Inner exception");
    co_return 0;
  };

  auto catchingTask = [&]() -> Task<int> {
    int val = co_await throwingTask();
    co_return val;  // Never reached
  }();

  EXPECT_THROW({ catchingTask.get(); }, std::runtime_error);
}

// Test: Multiple co_awaits with symmetric transfer
TEST(TaskTest, MultipleCoAwaitsWithSymmetricTransfer) {
  std::vector<int> execution_order;

  auto task1 = [&]() -> Task<int> {
    execution_order.push_back(1);
    co_return 10;
  };

  auto task2 = [&]() -> Task<int> {
    execution_order.push_back(2);
    co_return 20;
  };

  auto task3 = [&]() -> Task<int> {
    execution_order.push_back(3);
    co_return 30;
  };

  auto aggregator = [&]() -> Task<int> {
    execution_order.push_back(0);
    int a = co_await task1();
    int b = co_await task2();
    int c = co_await task3();
    co_return a + b + c;
  }();

  EXPECT_EQ(aggregator.get(), 60);

  // Execution order: aggregator starts, then task1, task2, task3
  std::vector<int> expected = {0, 1, 2, 3};
  EXPECT_EQ(execution_order, expected);
}
