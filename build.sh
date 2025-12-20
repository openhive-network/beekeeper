#!/bin/bash
# Build script for Beekeeper standalone

set -euo pipefail

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

echo "Building Beekeeper (${BUILD_TYPE})..."

# Initialize submodules if needed
if [ ! -f "hive/CMakeLists.txt" ]; then
    echo "Initializing submodules..."
    git submodule update --init --recursive
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
cmake ../hive \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -GNinja

# Build only beekeeper
ninja beekeeper

echo ""
echo "Build complete!"
echo "Binary: ${BUILD_DIR}/programs/beekeeper/beekeeper"
