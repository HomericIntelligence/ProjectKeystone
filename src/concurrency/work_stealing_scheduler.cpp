/**
 * @file work_stealing_scheduler.cpp
 * @brief Implementation of WorkStealingScheduler
 */

#include "concurrency/work_stealing_scheduler.hpp"
#include <sstream>

namespace keystone {
namespace concurrency {

WorkStealingScheduler::WorkStealingScheduler(size_t num_workers)
    : num_workers_(num_workers) {

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
        workers_.emplace_back([this, i]() {
            workerLoop(i);
        });
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

void WorkStealingScheduler::submitTo(size_t worker_index, std::function<void()> func) {
    if (worker_index >= num_workers_) {
        Logger::error("Invalid worker index: {} (max: {})", worker_index, num_workers_ - 1);
        return;
    }

    worker_queues_[worker_index]->push(WorkItem::makeFunction(std::move(func)));
}

void WorkStealingScheduler::submitTo(size_t worker_index, std::coroutine_handle<> handle) {
    if (worker_index >= num_workers_) {
        Logger::error("Invalid worker index: {} (max: {})", worker_index, num_workers_ - 1);
        return;
    }

    worker_queues_[worker_index]->push(WorkItem::makeCoroutine(handle));
}

void WorkStealingScheduler::shutdown() {
    if (!running_.load()) {
        return;  // Not running
    }

    Logger::info("WorkStealingScheduler shutting down...");

    // Set shutdown flag
    shutdown_requested_.store(true);

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

bool WorkStealingScheduler::isRunning() const {
    return running_.load();
}

size_t WorkStealingScheduler::getNumWorkers() const {
    return num_workers_;
}

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

    Logger::debug("Worker {} starting", worker_index);

    auto& own_queue = *worker_queues_[worker_index];

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
                    Logger::trace("Worker {} stole work from worker {}", worker_index, victim_idx);
                    break;
                }
            }
        }

        if (work.has_value() && work->valid()) {
            // Execute the work item
            Logger::trace("Worker {} executing work", worker_index);
            work->execute();
        } else {
            // No work available, sleep briefly to avoid busy-waiting
            std::this_thread::sleep_for(std::chrono::microseconds(100));
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
}

} // namespace concurrency
} // namespace keystone
