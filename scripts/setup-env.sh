#!/bin/bash
#
# Setup environment variables for Docker + Justfile
# This script ensures proper UID/GID and GIT_COMMIT values
#

set -e

echo "Setting up ProjectKeystone environment..."

# Get current user's UID and GID
CURRENT_UID=$(id -u)
CURRENT_GID=$(id -g)

# Get git commit hash (short) or use 'latest' if not in git repo
GIT_COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "latest")

echo "  UID: $CURRENT_UID"
echo "  GID: $CURRENT_GID"
echo "  GIT_COMMIT: $GIT_COMMIT"

# Create or update .env file with proper values
cat > .env << EOF
# .env
# Global environment variables for Docker + Justfile

# Git commit hash (short), fallback to 'latest'
GIT_COMMIT=${GIT_COMMIT}

# Default UID/GID for dev container (use host user's UID/GID)
BUILD_UID=${CURRENT_UID}
BUILD_GID=${CURRENT_GID}
EOF

echo "✓ Environment setup complete!"
echo ""
echo "You can now use:"
echo "  just build          # Build debug version"
echo "  just debug          # Build and enter dev container"
echo "  just test           # Run tests"
echo ""
echo "Or use docker-compose directly:"
echo "  docker-compose up -d dev    # Start dev container"
echo "  docker-compose exec dev /bin/bash  # Enter container"
