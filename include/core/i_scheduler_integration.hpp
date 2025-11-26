#pragma once

// Forward declaration
namespace keystone {
namespace concurrency {
class WorkStealingScheduler;
}
}  // namespace keystone

namespace keystone {
namespace core {

/**
 * @brief Interface for scheduler integration
 *
 * Separates scheduler lifecycle operations from agent registry and message
 * routing, following the Interface Segregation Principle.
 *
 * Responsibilities:
 * - Set the scheduler for asynchronous operation
 * - Query the current scheduler
 *
 * This interface enables:
 * - Independent testing of scheduler integration
 * - Optional async behavior (scheduler can be nullptr)
 * - Clearer separation of concerns in concurrency code
 * - Different scheduler implementations
 */
class ISchedulerIntegration {
 public:
  virtual ~ISchedulerIntegration() = default;

  /**
   * @brief Set the work-stealing scheduler for async message routing
   *
   * When scheduler is set, message routing can use asynchronous delivery
   * via the scheduler instead of synchronous delivery.
   *
   * @param scheduler Pointer to scheduler (must outlive this object, can be
   * nullptr)
   */
  virtual void setScheduler(
      concurrency::WorkStealingScheduler* scheduler) = 0;

  /**
   * @brief Get the current scheduler
   *
   * @return Pointer to scheduler (may be nullptr if not set)
   */
  virtual concurrency::WorkStealingScheduler* getScheduler() const = 0;
};

}  // namespace core
}  // namespace keystone
