#pragma once

#include <chrono>
#include <optional>
#include <queue>
#include <map>
#include <mutex>
#include <random>
#include <atomic>
#include <functional>

namespace keystone {
namespace simulation {

/**
 * @brief Simulated network for testing distributed work-stealing
 *
 * Phase D.3: Simulates network latency, message serialization overhead,
 * and optional bandwidth limiting for realistic distributed testing.
 *
 * Example:
 * @code
 * SimulatedNetwork::Config config{
 *     .min_latency = 100us,
 *     .max_latency = 500us
 * };
 * SimulatedNetwork network(config);
 * network.send(0, 1, work_item);  // Send from node 0 to node 1 with latency
 * @endcode
 */
class SimulatedNetwork {
public:
    /**
     * @brief Network configuration parameters
     */
    struct Config {
        std::chrono::microseconds min_latency{100};   ///< Minimum network latency
        std::chrono::microseconds max_latency{1000};  ///< Maximum network latency
        size_t bandwidth_mbps{1000};                   ///< Bandwidth in Mbps (unused for now)
        double packet_loss_rate{0.0};                  ///< Packet loss probability [0.0, 1.0]
    };

    /**
     * @brief Message wrapper for network transport
     */
    struct NetworkMessage {
        size_t from_node;                              ///< Source node ID
        size_t to_node;                                ///< Destination node ID
        std::function<void()> work;                    ///< Work item payload
        std::chrono::steady_clock::time_point sent_at; ///< Send timestamp
        std::chrono::steady_clock::time_point deliver_at; ///< Delivery timestamp
    };

    /**
     * @brief Construct a simulated network with default configuration
     */
    SimulatedNetwork();

    /**
     * @brief Construct a simulated network
     * @param config Network configuration (latency, bandwidth, etc.)
     */
    explicit SimulatedNetwork(Config config);

    /**
     * @brief Send work from one node to another (with latency)
     *
     * Injects network latency by scheduling delivery in the future.
     * Simulates packet loss based on config.packet_loss_rate.
     *
     * @param from_node Source node ID
     * @param to_node Destination node ID
     * @param work Work item to send
     */
    void send(size_t from_node, size_t to_node, std::function<void()> work);

    /**
     * @brief Receive work for a specific node (non-blocking)
     *
     * Returns work if latency period has elapsed, nullopt otherwise.
     *
     * @param node_id Destination node ID
     * @return Work item if ready for delivery, nullopt otherwise
     */
    std::optional<std::function<void()>> receive(size_t node_id);

    /**
     * @brief Get total messages sent (including dropped)
     * @return Message count
     */
    size_t getTotalMessages() const { return total_messages_.load(); }

    /**
     * @brief Get messages successfully delivered
     * @return Delivered message count
     */
    size_t getDeliveredMessages() const { return delivered_messages_.load(); }

    /**
     * @brief Get messages dropped due to packet loss
     * @return Dropped message count
     */
    size_t getDroppedMessages() const { return dropped_messages_.load(); }

    /**
     * @brief Get average network latency in microseconds
     * @return Average latency (or 0.0 if no messages)
     */
    double getAverageLatencyUs() const;

    /**
     * @brief Get number of pending messages (in-flight)
     * @return Pending message count
     */
    size_t getPendingMessages() const;

    /**
     * @brief Reset all statistics
     */
    void resetStats();

private:
    Config config_;  ///< Network configuration

    // Message queues per destination node
    std::map<size_t, std::queue<NetworkMessage>> message_queues_;
    mutable std::mutex queue_mutex_;

    // Statistics
    std::atomic<size_t> total_messages_{0};
    std::atomic<size_t> delivered_messages_{0};
    std::atomic<size_t> dropped_messages_{0};
    std::atomic<uint64_t> total_latency_us_{0};  // Sum of all latencies

    // Random number generator for latency and packet loss
    std::mt19937 rng_;
    std::uniform_int_distribution<int> latency_dist_;
    std::uniform_real_distribution<double> loss_dist_;

    /**
     * @brief Generate random latency within configured range
     * @return Latency in microseconds
     */
    std::chrono::microseconds generateLatency();

    /**
     * @brief Check if packet should be dropped (based on loss rate)
     * @return true if packet should be dropped
     */
    bool shouldDropPacket();
};

} // namespace simulation
} // namespace keystone
