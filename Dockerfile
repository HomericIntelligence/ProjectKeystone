# ProjectKeystone HMAS - C++20 Build Environment
# Multi-stage build for efficient image size

# Stage 1: Build environment
FROM ubuntu:24.04 AS builder

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++-12 \
    gcc-12 \
    git \
    libgtest-dev \
    ninja-build \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && rm -rf /var/lib/apt/lists/*

# Verify C++20 support
RUN g++ --version && cmake --version

# Set working directory
WORKDIR /workspace

# Copy project files
COPY CMakeLists.txt ./
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/
COPY benchmarks/ ./benchmarks/
COPY fuzz/ ./fuzz/

# Build the project
RUN mkdir -p build && cd build \
    && cmake -G Ninja .. \
    && ninja

# Stage 2: Runtime environment (smaller image)
FROM ubuntu:24.04 AS runtime

# Install only runtime dependencies
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Copy built executables from builder
COPY --from=builder /workspace/build/phase1_e2e_tests /usr/local/bin/

# Set working directory
WORKDIR /app

# Default command: run tests
CMD ["phase1_e2e_tests"]

# Stage 3: Production environment (Kubernetes deployment)
FROM ubuntu:24.04 AS production

# Install runtime dependencies + Python3 for health checks
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    python3 \
    python3-minimal \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user for security
RUN groupadd -g 1000 hmas && \
    useradd -r -u 1000 -g hmas hmas && \
    mkdir -p /app && \
    chown -R hmas:hmas /app

# Copy built executables from builder
COPY --from=builder /workspace/build/phase1_e2e_tests /usr/local/bin/
COPY --from=builder /workspace/build/phase2_e2e_tests /usr/local/bin/
COPY --from=builder /workspace/build/phase3_e2e_tests /usr/local/bin/
COPY --from=builder /workspace/build/phase_a_e2e_tests /usr/local/bin/
COPY --from=builder /workspace/build/phase_4_e2e_tests /usr/local/bin/
COPY --from=builder /workspace/build/phase_5_e2e_tests /usr/local/bin/
COPY --from=builder /workspace/build/phase_d3_e2e_tests /usr/local/bin/

# Copy server wrapper script
COPY scripts/hmas-server.sh /usr/local/bin/hmas-server.sh
RUN chmod +x /usr/local/bin/hmas-server.sh

# Set working directory
WORKDIR /app

# Switch to non-root user
USER hmas

# Expose ports
EXPOSE 8080 9090 50051

# Health check
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD python3 -c "import urllib.request; urllib.request.urlopen('http://localhost:8080/healthz')" || exit 1

# Default command: run HMAS server
CMD ["/usr/local/bin/hmas-server.sh"]

# Stage 4: Development environment (for iterative development)
FROM builder AS development

# Install additional development tools
RUN apt-get update && apt-get install -y \
    gdb \
    valgrind \
    clang-format \
    clang-tidy \
    cppcheck \
    lcov \
    bc \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Keep the build directory
# This stage is for development with mounted volumes

CMD ["/bin/bash"]
