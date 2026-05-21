#!/usr/bin/env bash
#
# Build the C++ side of beekeeper_rust (fc_crypto_bridge + its transitive
# dependencies) and then run `cargo` with the env vars build.rs needs to
# resolve fc symbols at link time.
#
# Usage:
#   ./build_for_cargo.sh               # defaults to `cargo test`
#   ./build_for_cargo.sh build
#   ./build_for_cargo.sh build --release
#   ./build_for_cargo.sh test -- --nocapture
#   ./build_for_cargo.sh check
#
# Configuration via env vars:
#   BUILD_DIR    Out-of-tree CMake build directory (default: <repo>/build)
#   BUILD_TYPE   CMake build type (default: Release)
#
# Linux-only assumptions: pthread, rt, dl; system Boost (libboost_*) and
# OpenSSL (-lssl -lcrypto) installed. On macOS drop -lrt and adjust
# Boost lib names if needed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

# ── 1. Configure CMake if not already done ────────────────────────────────
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo ">>> Configuring CMake in $BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    GENERATOR="Unix Makefiles"
    if command -v ninja >/dev/null 2>&1; then GENERATOR="Ninja"; fi
    cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -G "$GENERATOR"
fi

# ── 2. Build the fc_crypto_bridge target (transitively builds fc, secp256k1) ─
echo ">>> Building fc_crypto_bridge in $BUILD_DIR"
cmake --build "$BUILD_DIR" --target fc_crypto_bridge --parallel

# ── 3. Locate the static libs by name ─────────────────────────────────────
find_lib() {
    find "$BUILD_DIR" -name "lib$1.a" -print -quit 2>/dev/null || true
}
require_lib() {
    local p; p="$(find_lib "$1")"
    if [ -z "$p" ]; then
        echo "ERROR: lib$1.a not found under $BUILD_DIR" >&2
        exit 1
    fi
    echo "$p"
}

FC_CRYPTO_BRIDGE_LIB="$(require_lib fc_crypto_bridge)"
FC_LIB="$(require_lib fc)"
SECP_LIB="$(require_lib secp256k1)"

# ── 4. Export env vars build.rs reads ─────────────────────────────────────
export BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR="$(dirname "$FC_CRYPTO_BRIDGE_LIB")"
export BEEKEEPER_FC_CRYPTO_BRIDGE_INCLUDE_DIR="$REPO_ROOT/programs/beekeeper/fc_crypto_bridge/include"

# fc, secp256k1, Boost (minimal-fc set), OpenSSL, compression, system libs.
# Order matters: high-level → low-level for static link resolution.
BEEKEEPER_FC_LINK_FLAGS=""
BEEKEEPER_FC_LINK_FLAGS+=" -L$(dirname "$FC_LIB") -lfc"
BEEKEEPER_FC_LINK_FLAGS+=" -L$(dirname "$SECP_LIB") -lsecp256k1"
BEEKEEPER_FC_LINK_FLAGS+=" -lboost_chrono -lboost_date_time -lboost_filesystem -lboost_system -lboost_thread"
BEEKEEPER_FC_LINK_FLAGS+=" -lssl -lcrypto"
BEEKEEPER_FC_LINK_FLAGS+=" -lz -lbz2"
BEEKEEPER_FC_LINK_FLAGS+=" -lpthread -lrt -ldl"
export BEEKEEPER_FC_LINK_FLAGS

echo ">>> BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR=$BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR"
echo ">>> BEEKEEPER_FC_LINK_FLAGS=$BEEKEEPER_FC_LINK_FLAGS"

# ── 5. Run cargo ──────────────────────────────────────────────────────────
cd "$SCRIPT_DIR"
if [ $# -eq 0 ]; then
    exec cargo test
else
    exec cargo "$@"
fi
