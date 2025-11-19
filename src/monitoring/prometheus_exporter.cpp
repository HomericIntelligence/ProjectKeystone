#include "monitoring/prometheus_exporter.hpp"
#include "core/metrics.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iostream>

namespace keystone {
namespace monitoring {

PrometheusExporter::PrometheusExporter(int port) : port_(port) {}

PrometheusExporter::~PrometheusExporter() {
    stop();
}

bool PrometheusExporter::start() {
    if (running_.load()) {
        return false;  // Already running
    }

    // Create socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // Set socket options (reuse address)
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // Bind to port
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind to port " << port_ << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // Listen for connections
    if (listen(server_fd_, 10) < 0) {
        std::cerr << "Failed to listen on port " << port_ << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    // Start server thread
    running_.store(true);
    server_thread_ = std::make_unique<std::thread>(&PrometheusExporter::serverLoop, this);

    std::cout << "Prometheus exporter started on port " << port_ << std::endl;
    return true;
}

void PrometheusExporter::stop() {
    if (!running_.load()) {
        return;  // Not running
    }

    running_.store(false);

    // Close server socket to unblock accept()
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }

    // Wait for server thread to finish
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }

    std::cout << "Prometheus exporter stopped" << std::endl;
}

bool PrometheusExporter::isRunning() const {
    return running_.load();
}

int PrometheusExporter::getPort() const {
    return port_;
}

void PrometheusExporter::serverLoop() {
    while (running_.load()) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);

        // Accept connection
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_address, &client_len);
        if (client_fd < 0) {
            if (running_.load()) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }

        // Handle request
        handleRequest(client_fd);

        // Close client connection
        close(client_fd);
    }
}

void PrometheusExporter::handleRequest(int client_fd) {
    // Read HTTP request (we don't parse it, just respond)
    char buffer[1024];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        return;
    }

    // Check if this is a GET request to /metrics
    buffer[bytes_read] = '\0';
    std::string request(buffer);
    bool is_metrics_request = (request.find("GET /metrics") != std::string::npos);

    if (is_metrics_request) {
        // Generate metrics
        std::string metrics = generateMetrics();

        // Send HTTP response
        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: text/plain; version=0.0.4\r\n";
        response << "Content-Length: " << metrics.size() << "\r\n";
        response << "\r\n";
        response << metrics;

        std::string response_str = response.str();
        write(client_fd, response_str.c_str(), response_str.size());
    } else {
        // Send 404 for other paths
        std::string not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        write(client_fd, not_found.c_str(), not_found.size());
    }
}

std::string PrometheusExporter::generateMetrics() {
    std::ostringstream ss;

    // Get metrics from singleton
    auto& metrics = core::Metrics::getInstance();

    // Messages sent (counters by priority)
    auto priority_stats = metrics.getPriorityStats();
    ss << "# HELP hmas_messages_total Total number of messages sent by priority\n";
    ss << "# TYPE hmas_messages_total counter\n";
    ss << "hmas_messages_total{priority=\"high\"} " << priority_stats.high_count << "\n";
    ss << "hmas_messages_total{priority=\"normal\"} " << priority_stats.normal_count << "\n";
    ss << "hmas_messages_total{priority=\"low\"} " << priority_stats.low_count << "\n";

    // Messages processed (counter)
    ss << "# HELP hmas_messages_processed_total Total number of messages processed\n";
    ss << "# TYPE hmas_messages_processed_total counter\n";
    ss << "hmas_messages_processed_total " << metrics.getTotalMessagesProcessed() << "\n";

    // Message latency (gauge - average)
    auto avg_latency = metrics.getAverageLatencyUs();
    if (avg_latency.has_value()) {
        ss << "# HELP hmas_message_latency_microseconds Average message processing latency\n";
        ss << "# TYPE hmas_message_latency_microseconds gauge\n";
        ss << "hmas_message_latency_microseconds " << avg_latency.value() << "\n";
    }

    // Queue depth (gauge - maximum across all agents)
    ss << "# HELP hmas_queue_depth_max Maximum queue depth across all agents\n";
    ss << "# TYPE hmas_queue_depth_max gauge\n";
    ss << "hmas_queue_depth_max " << metrics.getMaxQueueDepth() << "\n";

    // Worker utilization (gauge - percentage)
    ss << "# HELP hmas_worker_utilization_percent Worker utilization percentage\n";
    ss << "# TYPE hmas_worker_utilization_percent gauge\n";
    ss << "hmas_worker_utilization_percent " << metrics.getWorkerUtilization() << "\n";

    // Messages per second (gauge)
    ss << "# HELP hmas_messages_per_second Message throughput\n";
    ss << "# TYPE hmas_messages_per_second gauge\n";
    ss << "hmas_messages_per_second " << metrics.getMessagesPerSecond() << "\n";

    // Deadline misses (counter)
    ss << "# HELP hmas_deadline_misses_total Total number of deadline misses\n";
    ss << "# TYPE hmas_deadline_misses_total counter\n";
    ss << "hmas_deadline_misses_total " << metrics.getTotalDeadlineMisses() << "\n";

    // Deadline miss time (gauge - average)
    auto avg_miss = metrics.getAverageDeadlineMissMs();
    if (avg_miss.has_value()) {
        ss << "# HELP hmas_deadline_miss_milliseconds Average deadline miss time\n";
        ss << "# TYPE hmas_deadline_miss_milliseconds gauge\n";
        ss << "hmas_deadline_miss_milliseconds " << avg_miss.value() << "\n";
    }

    // Uptime (gauge - seconds since start)
    static auto start_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    ss << "# HELP hmas_uptime_seconds HMAS uptime in seconds\n";
    ss << "# TYPE hmas_uptime_seconds gauge\n";
    ss << "hmas_uptime_seconds " << uptime_seconds << "\n";

    // Health status (gauge - always 1 if responding)
    ss << "# HELP hmas_up HMAS health status (1=up, 0=down)\n";
    ss << "# TYPE hmas_up gauge\n";
    ss << "hmas_up 1\n";

    return ss.str();
}

} // namespace monitoring
} // namespace keystone
