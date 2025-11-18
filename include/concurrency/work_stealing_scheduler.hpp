#pragma once

#include "concurrency/work_stealing_queue.hpp"
#include "concurrency/pull_or_steal.hpp"
#include "concurrency/logger.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <memory>
#include <functional>

namespace keystone {
namespace concurrency {

/**
 * @brief WorkStealingScheduler - Orchestrates work-stealing across worker threads
 *
 * This scheduler manages a fixed pool of worker threads, each with its own
 * work-stealing queue. Workers try to pop from their own queue first (LIFO),
 * and steal from other workers when idle (FIFO).
 *
 * Architecture:
 * - Each worker has a dedicated queue (cache-friendly)
 * - Workers run a loop: pull from own queue or steal from others
 * - Round-robin work submission for load balancing
 * - Graceful shutdown with work completion
 *
 * Usage:
 *   WorkStealingScheduler scheduler(4);  // 4 workers
 *   scheduler.start();
 *
 *   scheduler.submit([]() { });  // Submit function
 *   scheduler.submit(coroutine_handle);  // Submit coroutine
 *
 *   scheduler.shutdown();
 *
 * Integration with Agents:
 * - Each agent coroutine submitted to scheduler
 * - LogContext set per worker thread
 * - Messages routed to appropriate worker queues
 */
class WorkStealingScheduler {
public:
    /**
     * @brief Construct a work-stealing scheduler
     *
     * @param num_workers Number of worker threads (default: hardware_concurrency)
     */
    explicit WorkStealingScheduler(size_t num_workers = std::thread::hardware_concurrency());

    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~WorkStealingScheduler();

    // Non-copyable, non-movable (manages threads)
    WorkStealingScheduler(const WorkStealingScheduler&) = delete;
    WorkStealingScheduler& operator=(const WorkStealingScheduler&) = delete;
    WorkStealingScheduler(WorkStealingScheduler&&) = delete;
    WorkStealingScheduler& operator=(WorkStealingScheduler&&) = delete;

    /**
     * @brief Start all worker threads
     *
     * Must be called before submitting work.
     */
    void start();

    /**
     * @brief Submit a function for execution
     *
     * @param func Function to execute
     */
    void submit(std::function<void()> func);

    /**
     * @brief Submit a coroutine handle for execution
     *
     * @param handle Coroutine handle to resume
     */
    void submit(std::coroutine_handle<> handle);

    /**
     * @brief Submit work to a specific worker queue
     *
     * Useful for affinity-based scheduling (e.g., agent-to-worker mapping)
     *
     * @param worker_index Target worker index
     * @param func Function to execute
     */
    void submitTo(size_t worker_index, std::function<void()> func);

    /**
     * @brief Submit coroutine to a specific worker queue
     *
     * @param worker_index Target worker index
     * @param handle Coroutine handle to resume
     */
    void submitTo(size_t worker_index, std::coroutine_handle<> handle);

    /**
     * @brief Request graceful shutdown
     *
     * Sets shutdown flag and waits for all workers to complete current work.
     * Blocks until all threads have exited.
     */
    void shutdown();

    /**
     * @brief Check if scheduler is running
     */
    bool isRunning() const;

    /**
     * @brief Get number of worker threads
     */
    size_t getNumWorkers() const;

    /**
     * @brief Get approximate total work items across all queues
     *
     * Note: Approximate due to lock-free nature
     */
    size_t getApproximateWorkCount() const;

private:
    /**
     * @brief Worker thread main loop
     *
     * Each worker:
     * 1. Sets thread-local LogContext
     * 2. Loops: co_await PullOrSteal
     * 3. Executes work items
     * 4. Exits on shutdown
     *
     * @param worker_index This worker's index
     */
    void workerLoop(size_t worker_index);

    /**
     * @brief Get next worker index for round-robin submission
     */
    size_t getNextWorkerIndex();

    size_t num_workers_;
    std::vector<std::unique_ptr<WorkStealingQueue>> worker_queues_;
    std::vector<WorkStealingQueue*> queue_pointers_;  // For PullOrSteal
    std::vector<std::thread> workers_;

    std::atomic<bool> shutdown_requested_{false};
    std::atomic<bool> running_{false};
    std::atomic<size_t> next_worker_index_{0};  // Round-robin counter
};

} // namespace concurrency
} // namespace keystone
