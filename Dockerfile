# ProjectKeystone HMAS - C++20 Build Environment
# Multi-stage build for efficient image size

# Stage 1: Build environment
FROM ubuntu:22.04 as builder

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

# Build the project
RUN mkdir -p build && cd build \
    && cmake -G Ninja .. \
    && ninja

# Stage 2: Runtime environment (smaller image)
FROM ubuntu:22.04 as runtime

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

# Stage 3: Development environment (for iterative development)
FROM builder as development

# Install additional development tools
RUN apt-get update && apt-get install -y \
    gdb \
    valgrind \
    clang-format \
    clang-tidy \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Keep the build directory
# This stage is for development with mounted volumes

CMD ["/bin/bash"]
