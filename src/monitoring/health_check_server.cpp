#include "monitoring/health_check_server.hpp"

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
#include <utility>  // For std::exchange

#include "core/config.hpp"

namespace keystone {
namespace monitoring {

/**
 * @brief RAII wrapper for socket file descriptors
 *
 * Ensures sockets are always closed, even on error paths.
 */
class SocketHandle {
 public:
  explicit SocketHandle(int fd) : fd_(fd) {}

  ~SocketHandle() {
    if (fd_ >= 0) {
      close(fd_);
    }
  }

  // Non-copyable, movable
  SocketHandle(const SocketHandle&) = delete;
  SocketHandle& operator=(const SocketHandle&) = delete;

  SocketHandle(SocketHandle&& other) noexcept
      : fd_(std::exchange(other.fd_, -1)) {}

  SocketHandle& operator=(SocketHandle&& other) noexcept {
    if (this != &other) {
      if (fd_ >= 0) {
        close(fd_);
      }
      fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
  }

  int get() const { return fd_; }
  bool valid() const { return fd_ >= 0; }

 private:
  int fd_;
};

HealthCheckServer::HealthCheckServer(int port, ReadinessCheck readiness_check)
    : port_(port), readiness_check_(std::move(readiness_check)) {}

HealthCheckServer::~HealthCheckServer() { stop(); }

bool HealthCheckServer::start() {
  if (running_.load()) {
    return false;  // Already running
  }

  // Create socket
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    std::cerr << "HealthCheckServer: Failed to create socket" << std::endl;
    return false;
  }

  // Set socket options (reuse address)
  int opt = 1;
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::cerr << "HealthCheckServer: Failed to set socket options" << std::endl;
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
    std::cerr << "HealthCheckServer: Failed to bind to port " << port_
              << std::endl;
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }

  // Listen for connections
  if (listen(server_fd_, core::Config::HTTP_MAX_PENDING_CONNECTIONS) < 0) {
    std::cerr << "HealthCheckServer: Failed to listen on port " << port_
              << std::endl;
    close(server_fd_);
    server_fd_ = -1;
    return false;
  }

  // Start server thread
  running_.store(true);
  server_thread_ =
      std::make_unique<std::thread>(&HealthCheckServer::serverLoop, this);

  std::cout << "Health check server started on port " << port_ << std::endl;
  return true;
}

void HealthCheckServer::stop() {
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

  std::cout << "Health check server stopped" << std::endl;
}

bool HealthCheckServer::isRunning() const { return running_.load(); }

int HealthCheckServer::getPort() const { return port_; }

void HealthCheckServer::setReadinessCheck(ReadinessCheck check) {
  readiness_check_ = std::move(check);
}

void HealthCheckServer::serverLoop() {
  while (running_.load()) {
    // Use poll() with timeout to check for incoming connections
    // This allows periodic checking of running_ flag without blocking indefinitely
    struct pollfd pfd;
    pfd.fd = server_fd_;
    pfd.events = POLLIN;
    pfd.revents = 0;

    // Poll with 100ms timeout - allows responsive shutdown
    int poll_result = poll(&pfd, 1, 100);

    if (poll_result < 0) {
      // Poll error
      if (running_.load()) {
        std::cerr << "HealthCheckServer: Poll failed" << std::endl;
      }
      break;
    }

    if (poll_result == 0) {
      // Timeout - no incoming connections, loop again to check running_ flag
      continue;
    }

    // Data available - proceed with accept()
    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);

    int client_fd =
        accept(server_fd_, (struct sockaddr*)&client_address, &client_len);
    if (client_fd < 0) {
      if (running_.load()) {
        std::cerr << "HealthCheckServer: Accept failed" << std::endl;
      }
      continue;
    }

    // Use RAII wrapper - socket closed automatically on scope exit
    SocketHandle client_socket(client_fd);

    // Set socket read timeout to prevent slowloris attacks
    struct timeval timeout;
    timeout.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(
                         core::Config::HTTP_READ_TIMEOUT)
                         .count();
    timeout.tv_usec = 0;

    if (setsockopt(client_socket.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout,
                   sizeof(timeout)) < 0) {
      std::cerr << "HealthCheckServer: Failed to set socket read timeout"
                << std::endl;
      continue;  // Still try to handle request without timeout
    }

    // Handle request
    handleRequest(client_socket.get());

    // Socket automatically closed by SocketHandle destructor
  }
}

void HealthCheckServer::handleRequest(int client_fd) {
  // Use std::array for bounds safety
  std::array<char, core::Config::HTTP_REQUEST_BUFFER_SIZE> buffer;
  ssize_t bytes_read = read(client_fd, buffer.data(), buffer.size() - 1);

  // Validate read result and reject invalid requests
  if (bytes_read < 0) {
    // Read error (including timeout)
    return;
  }

  if (bytes_read == 0) {
    // Empty request - connection closed by client
    return;
  }

  // Validate minimum request size (at least "GET /")
  if (bytes_read < 5) {
    std::string bad_request =
        "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
    [[maybe_unused]] auto result =
        write(client_fd, bad_request.c_str(), bad_request.size());
    return;
  }

  // Clamp to safe range to prevent buffer overflow
  if (bytes_read >= static_cast<ssize_t>(buffer.size())) {
    bytes_read = buffer.size() - 1;
  }

  buffer[bytes_read] = '\0';
  std::string request(buffer.data());

  // Validate HTTP method (only accept GET)
  if (request.substr(0, 3) != "GET") {
    std::string method_not_allowed =
        "HTTP/1.1 405 Method Not Allowed\r\n"
        "Allow: GET\r\n"
        "Content-Length: 0\r\n\r\n";
    [[maybe_unused]] auto result =
        write(client_fd, method_not_allowed.c_str(), method_not_allowed.size());
    return;
  }

  // Check which endpoint is requested
  bool is_liveness = (request.find("GET /healthz") != std::string::npos);
  bool is_readiness = (request.find("GET /ready") != std::string::npos);

  if (is_liveness) {
    // Liveness probe - always return 200 OK if process is alive
    std::string body = generateLivenessResponse();

    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "\r\n";
    response << body;

    std::string response_str = response.str();
    [[maybe_unused]] auto result =
        write(client_fd, response_str.c_str(), response_str.size());

  } else if (is_readiness) {
    // Readiness probe - check if system is ready
    bool ready = true;
    if (readiness_check_) {
      ready = readiness_check_();
    }

    std::string body = generateReadinessResponse(ready);
    std::string status_line = ready ? "HTTP/1.1 200 OK\r\n"
                                    : "HTTP/1.1 503 Service Unavailable\r\n";

    std::ostringstream response;
    response << status_line;
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "\r\n";
    response << body;

    std::string response_str = response.str();
    [[maybe_unused]] auto result =
        write(client_fd, response_str.c_str(), response_str.size());

  } else {
    // Send 404 for other paths
    std::string not_found =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 27\r\n"
        "\r\n"
        "{\"error\":\"endpoint not found\"}";
    [[maybe_unused]] auto result =
        write(client_fd, not_found.c_str(), not_found.size());
  }
}

std::string HealthCheckServer::generateLivenessResponse() {
  return "{\"status\":\"healthy\"}";
}

std::string HealthCheckServer::generateReadinessResponse(bool ready) {
  if (ready) {
    return "{\"status\":\"ready\"}";
  } else {
    return "{\"status\":\"not_ready\"}";
  }
}

}  // namespace monitoring
}  // namespace keystone
