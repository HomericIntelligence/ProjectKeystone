/**
 * @file task.cpp
 * @brief Implementation file for Task<T> coroutine type
 *
 * Note: Task<T> is primarily a header-only template class.
 * This file exists to satisfy CMake build requirements and may contain
 * explicit template instantiations or helper functions in the future.
 */

#include "concurrency/task.hpp"

// Explicit template instantiations for common types
namespace keystone {
namespace concurrency {

// Common instantiations to reduce compile time
template class Task<int>;
template class Task<std::string>;
template class Task<void>;

}  // namespace concurrency
}  // namespace keystone
