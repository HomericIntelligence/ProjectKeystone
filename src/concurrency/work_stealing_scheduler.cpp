/**
 * @file work_stealing_scheduler.cpp
 * @brief Implementation of WorkStealingScheduler
 */

#include "concurrency/work_stealing_scheduler.hpp"

#include <sstream>

#include "concurrency/scheduler_accessor.hpp"
#include "core/config.hpp"  // FIX m3: Centralized configuration

// Phase D: CPU affinity support (Linux-specific)
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace keystone {
namespace concurrency {

WorkStealingScheduler::WorkStealingScheduler(size_t num_workers,
                                             bool enable_cpu_affinity)
    : num_workers_(num_workers), enable_cpu_affinity_(enable_cpu_affinity) {
  // FIX P2-10: Enforce maximum worker thread limit to prevent DoS
  if (num_workers_ > core::Config::MAX_WORKER_THREADS) {
    throw std::invalid_argument(
        "Too many worker threads requested: " + std::to_string(num_workers_) +
        " (max: " + std::to_string(core::Config::MAX_WORKER_THREADS) + ")");
  }

  if (num_workers_ == 0) {
    num_workers_ = 1;  // At least one worker
  }

  // Create worker queues
  worker_queues_.reserve(num_workers_);
  queue_pointers_.reserve(num_workers_);

  for (size_t i = 0; i < num_workers_; ++i) {
    worker_queues_.push_back(std::make_unique<WorkStealingQueue>());
    queue_pointers_.push_back(worker_queues_[i].get());
  }
}

WorkStealingScheduler::~WorkStealingScheduler() {
  if (running_.load()) {
    shutdown();
  }
}

void WorkStealingScheduler::start() {
  if (running_.exchange(true)) {
    // Already running
    return;
  }

  shutdown_requested_.store(false);
  workers_.reserve(num_workers_);

  Logger::info("WorkStealingScheduler starting with {} workers", num_workers_);

  // Launch worker threads
  for (size_t i = 0; i < num_workers_; ++i) {
    workers_.emplace_back([this, i]() { workerLoop(i); });
  }

  Logger::info("WorkStealingScheduler started");
}

void WorkStealingScheduler::submit(std::function<void()> func) {
  size_t worker_idx = getNextWorkerIndex();
  submitTo(worker_idx, std::move(func));
}

void WorkStealingScheduler::submit(std::coroutine_handle<> handle) {
  size_t worker_idx = getNextWorkerIndex();
  submitTo(worker_idx, handle);
}

void WorkStealingScheduler::submitTo(size_t worker_index,
                                     std::function<void()> func) {
  if (worker_index >= num_workers_) {
    Logger::error("Invalid worker index: {} (max: {})", worker_index,
                  num_workers_ - 1);
    return;
  }

  worker_queues_[worker_index]->push(WorkItem::makeFunction(std::move(func)));
}

void WorkStealingScheduler::submitTo(size_t worker_index,
                                     std::coroutine_handle<> handle) {
  if (worker_index >= num_workers_) {
    Logger::error("Invalid worker index: {} (max: {})", worker_index,
                  num_workers_ - 1);
    return;
  }

  worker_queues_[worker_index]->push(WorkItem::makeCoroutine(handle));
}

void WorkStealingScheduler::shutdown() {
  if (!running_.load()) {
    return;  // Not running
  }

  Logger::info("WorkStealingScheduler shutting down...");

  // Set shutdown flag and notify all workers
  // FIX Issue #18: Wake up all sleeping workers immediately
  {
    std::lock_guard<std::mutex> lock(shutdown_mutex_);
    shutdown_requested_.store(true);
  }
  shutdown_cv_.notify_all();

  // Wait for all workers to finish
  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }

  workers_.clear();
  running_.store(false);

  Logger::info("WorkStealingScheduler shutdown complete");
}

bool WorkStealingScheduler::isRunning() const { return running_.load(); }

size_t WorkStealingScheduler::getNumWorkers() const { return num_workers_; }

size_t WorkStealingScheduler::getApproximateWorkCount() const {
  size_t total = 0;
  for (const auto& queue : worker_queues_) {
    total += queue->size_approx();
  }
  return total;
}

size_t WorkStealingScheduler::getNextWorkerIndex() {
  // Round-robin: atomic increment and modulo
  size_t idx = next_worker_index_.fetch_add(1, std::memory_order_relaxed);
  return idx % num_workers_;
}

void WorkStealingScheduler::workerLoop(size_t worker_index) {
  // Set thread-local logging context
  std::ostringstream oss;
  oss << "worker_" << worker_index;
  LogContext::set(oss.str(), static_cast<int>(worker_index), "scheduler");

  // Issue #19: Register this scheduler in thread-local storage
  // This allows Task<T>::await_suspend() to access the scheduler for async submission
  setCurrentScheduler(this);

  // Phase D: Set CPU affinity if enabled
  if (enable_cpu_affinity_) {
    setCPUAffinity(worker_index);
  }

  Logger::debug("Worker {} starting", worker_index);

  auto& own_queue = *worker_queues_[worker_index];

  // FIX M5: Adaptive exponential backoff instead of fixed 100μs sleep
  size_t idle_count = 0;

  // Main work loop
  while (!shutdown_requested_.load()) {
    // Try to get work: pop from own queue or steal from others
    auto work = own_queue.pop();

    if (!work.has_value()) {
      // Try stealing from other workers
      for (size_t i = 1; i < num_workers_; ++i) {
        size_t victim_idx = (worker_index + i) % num_workers_;
        work = worker_queues_[victim_idx]->steal();

        if (work.has_value()) {
          Logger::trace("Worker {} stole work from worker {}", worker_index,
                        victim_idx);
          break;
        }
      }
    }

    if (work.has_value() && work->valid()) {
      // Execute the work item
      Logger::trace("Worker {} executing work", worker_index);
      work->execute();
      idle_count = 0;  // Reset backoff on successful work
    } else {
      // FIX M5: Adaptive exponential backoff
      // Start with 1μs, double each iteration up to
      // Config::SCHEDULER_MAX_BACKOFF_MICROSECONDS This reduces CPU waste while
      // maintaining low latency
      idle_count++;
      // FIX P3-05: Use Config constant instead of magic number
      size_t backoff_shift =
          std::min(idle_count, core::Config::SCHEDULER_MAX_BACKOFF_SHIFT);
      size_t sleep_us =
          std::min(1UL << backoff_shift,
                   core::Config::SCHEDULER_MAX_BACKOFF_MICROSECONDS);

      // FIX Issue #18: Use condition variable to wake up immediately on shutdown
      std::unique_lock<std::mutex> lock(shutdown_mutex_);
      shutdown_cv_.wait_for(lock, std::chrono::microseconds(sleep_us),
                           [this]() { return shutdown_requested_.load(); });
    }
  }

  // Drain remaining work from own queue before exiting
  Logger::debug("Worker {} draining queue before exit", worker_index);
  while (auto work = own_queue.pop()) {
    if (work->valid()) {
      work->execute();
    }
  }

  Logger::debug("Worker {} exiting", worker_index);
  LogContext::clear();

  // Issue #19: Clear the thread-local scheduler when worker exits
  setCurrentScheduler(nullptr);
}

void WorkStealingScheduler::setCPUAffinity(size_t worker_index) {
#ifdef __linux__
  // Linux: Use pthread_setaffinity_np()
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  // Map worker to CPU core (wrap around if more workers than cores)
  size_t cpu_id = worker_index % std::thread::hardware_concurrency();
  CPU_SET(cpu_id, &cpuset);

  pthread_t thread = pthread_self();
  int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

  if (result != 0) {
    Logger::warn("Worker {} failed to set CPU affinity to core {}: error {}",
                 worker_index, cpu_id, result);
  } else {
    Logger::debug("Worker {} pinned to CPU core {}", worker_index, cpu_id);
  }
#else
  // Other platforms: No-op (affinity not supported or not implemented)
  Logger::debug("Worker {}: CPU affinity not supported on this platform",
                worker_index);
  (void)worker_index;  // Suppress unused parameter warning
#endif
}

}  // namespace concurrency
}  // namespace keystone
