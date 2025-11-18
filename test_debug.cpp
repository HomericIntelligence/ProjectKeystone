#include "concurrency/task.hpp"
#include <iostream>

using namespace keystone::concurrency;

int main() {
    std::cout << "Test 1: Task<int>" << std::endl;
    auto task1 = []() -> Task<int> {
        std::cout << "  Inside Task<int> coroutine body" << std::endl;
        co_return 42;
    }();
    std::cout << "  After creating task1, done=" << task1.done() << std::endl;
    int result = task1.get();
    std::cout << "  After get(), result=" << result << ", done=" << task1.done() << std::endl;

    std::cout << "\nTest 2: Task<void>" << std::endl;
    bool executed = false;
    auto task2 = [&]() -> Task<void> {
        std::cout << "  Inside Task<void> coroutine body" << std::endl;
        executed = true;
        std::cout << "  Set executed=true" << std::endl;
        co_return;
    }();
    std::cout << "  After creating task2, executed=" << executed << ", done=" << task2.done() << std::endl;
    task2.get();
    std::cout << "  After get(), executed=" << executed << ", done=" << task2.done() << std::endl;

    return 0;
}
