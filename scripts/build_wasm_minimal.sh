#!/bin/bash
# Build the minimal WASM beekeeper module
# This produces a ~100-200KB WASM file instead of ~3MB

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build_wasm_minimal"

# Check for Emscripten
if ! command -v emcmake &> /dev/null; then
    echo "Error: emcmake not found. Please install and activate Emscripten."
    echo "       source /path/to/emsdk/emsdk_env.sh"
    exit 1
fi

echo "Building minimal WASM beekeeper..."
echo "Repository root: $REPO_ROOT"
echo "Build directory: $BUILD_DIR"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with Emscripten
emcmake cmake \
    -DCMAKE_BUILD_TYPE=Release \
    "${REPO_ROOT}/programs/beekeeper/beekeeper_wasm_minimal"

# Build
emmake make -j$(nproc)

# Report sizes
echo ""
echo "Build complete!"
echo "Output files:"
ls -lh beekeeper_minimal.js beekeeper_minimal.wasm 2>/dev/null || true

# Copy to the TypeScript source directory
OUTPUT_DIR="${REPO_ROOT}/programs/beekeeper/beekeeper_wasm/src/build_minimal"
mkdir -p "$OUTPUT_DIR"
cp beekeeper_minimal.js beekeeper_minimal.wasm "$OUTPUT_DIR/" 2>/dev/null || true

echo ""
echo "Files copied to: $OUTPUT_DIR"
