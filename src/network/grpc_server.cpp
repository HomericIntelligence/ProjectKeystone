#include "network/grpc_server.hpp"

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>

namespace keystone::network {

GrpcServer::GrpcServer(const GrpcServerConfig& config) : config_(config) {}

GrpcServer::~GrpcServer() {
  if (server_) {
    stop();
  }
}

void GrpcServer::registerService(grpc::Service* service) {
  if (started_) {
    throw std::runtime_error(
        "Cannot register service after server has started");
  }
  services_.push_back(service);
}

bool GrpcServer::start() {
  if (started_) {
    return false;
  }

  grpc::ServerBuilder builder;

  // Set server address
  builder.AddListeningPort(config_.server_address, buildCredentials());

  // Register all services
  for (auto* service : services_) {
    builder.RegisterService(service);
  }

  // Set max message size
  builder.SetMaxReceiveMessageSize(config_.max_message_size);
  builder.SetMaxSendMessageSize(config_.max_message_size);

  // Build and start server
  server_ = builder.BuildAndStart();

  if (server_) {
    started_ = true;
    return true;
  }

  return false;
}

void GrpcServer::stop(int grace_period_ms) {
  if (server_) {
    auto deadline = std::chrono::system_clock::now() +
                    std::chrono::milliseconds(grace_period_ms);
    server_->Shutdown(deadline);
    server_.reset();
    started_ = false;
  }
}

void GrpcServer::wait() {
  if (server_) {
    server_->Wait();
  }
}

std::shared_ptr<grpc::ServerCredentials> GrpcServer::buildCredentials() {
  if (config_.enable_tls) {
    // Load TLS credentials
    grpc::SslServerCredentialsOptions ssl_opts;
    grpc::SslServerCredentialsOptions::PemKeyCertPair key_cert_pair;

    // Read certificate and key files
    // Note: In production, implement proper file reading
    // For now, return insecure credentials
    // TODO: Implement TLS certificate loading
    return grpc::InsecureServerCredentials();
  } else {
    return grpc::InsecureServerCredentials();
  }
}

}  // namespace keystone::network
