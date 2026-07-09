#!/bin/bash
set -e

# Produces a single self-contained static library
#   target/prelink/<profile>/libbeekeeper_native-<target-triple>.a
# and installs it as lib/libbeekeeper_native.a, which build.rs links on every
# regular `cargo build` — consumers never compile the C++.
#
# Unlike wax's prelink_bundle.sh, the prelink itself (ld -r --gc-sections
# rooted at the cxx bridge symbols, strip, symbol localization,
# STB_GNU_UNIQUE renames) already happens inside build.rs's standalone mode;
# this script:
#   1. drives that build (BEEKEEPER_FROM_SOURCE=1 cargo build),
#   2. verifies the bundle: no leaked Boost/OpenSSL/zlib/bz2 requirements,
#      exported symbols are exactly the cxx bridge roots,
#   3. link-and-run smoke test against system libs only, proving the bundle
#      needs no Boost/OpenSSL/beekeeper checkout on the consuming machine,
#   4. installs the bundle into the crate at lib/libbeekeeper_native.a.
#
# This is the local implementation of the "prelink and shrink" step that CI
# will eventually run once per target triple.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

BEEKEEPER_PROFILE=${BEEKEEPER_PROFILE:-debug}

for arg in "$@"; do
  case "$arg" in
    release|debug) BEEKEEPER_PROFILE="$arg" ;;
    *) echo "Unknown argument: $arg" >&2; exit 1 ;;
  esac
done

CXX_BIN=${CXX:-c++}
TARGET_DIR="${CARGO_TARGET_DIR:-${SCRIPT_DIR}/target}"

# Builds the C++ from the beekeeper tree and prelinks it; regular
# `cargo build` never does this — it links the bundle this script installs.
CARGO_FLAGS=""
if [ "${BEEKEEPER_PROFILE}" = "release" ]; then
  CARGO_FLAGS="--release"
fi
(cd "${SCRIPT_DIR}" && BEEKEEPER_FROM_SOURCE=1 CARGO_TARGET_DIR="${TARGET_DIR}" \
  cargo build --quiet ${CARGO_FLAGS})

OUT_DIR=$(ls -dt "${TARGET_DIR}/${BEEKEEPER_PROFILE}"/build/hiveio-beekeeper-*/out 2>/dev/null | head -1)
if [ -z "${OUT_DIR}" ]; then
  echo "No build output under ${TARGET_DIR}/${BEEKEEPER_PROFILE}/build" >&2
  exit 1
fi

BUNDLE="${OUT_DIR}/prelink/beekeeper_native.o"
EXPORTS="${OUT_DIR}/prelink/exported_symbols.txt"
for f in "${BUNDLE}" "${EXPORTS}"; do
  if [ ! -f "${f}" ]; then
    echo "Prelink artifact not found: ${f}" >&2
    exit 1
  fi
done

ROOT_COUNT=$(wc -l < "${EXPORTS}")
if [ "${ROOT_COUNT}" -eq 0 ]; then
  echo "No cxxbridge symbols listed in ${EXPORTS}" >&2
  exit 1
fi

# Self-containment check: an undefined Boost/OpenSSL/zlib/bz2 symbol here
# means a static archive was incomplete and the requirement would leak to
# consumers.
LEAKED=$(nm -u "${BUNDLE}" | grep -cE 'boost|SSL_|EVP_|CRYPTO_|BZ2_|inflate|deflate' || true)
if [ "${LEAKED}" -ne 0 ]; then
  echo "Bundle leaks ${LEAKED} undefined Boost/OpenSSL/zlib/bz2 symbols:" >&2
  nm -u "${BUNDLE}" | grep -E 'boost|SSL_|EVP_|CRYPTO_|BZ2_|inflate|deflate' | head >&2
  exit 1
fi

EXPORTED=$(nm "${BUNDLE}" | awk '$2 == "T"' | wc -l)
if [ "${EXPORTED}" -ne "${ROOT_COUNT}" ]; then
  echo "Expected ${ROOT_COUNT} exported symbols, got ${EXPORTED}" >&2
  exit 1
fi

WORK_DIR="${TARGET_DIR}/prelink/${BEEKEEPER_PROFILE}"
mkdir -p "${WORK_DIR}"

TRIPLE=$(rustc -vV | awk '/^host:/ {print $2}')
ARTIFACT="${WORK_DIR}/libbeekeeper_native-${TRIPLE}.a"
rm -f "${ARTIFACT}"
ar rcs "${ARTIFACT}" "${BUNDLE}"

# The cxx crate compiles its C++ runtime (rust::cxxbridge1::String/Str/...)
# in its own build script and cargo links it into every consumer, so the
# bundle deliberately leaves those symbols undefined — the final binary must
# hold exactly one copy, the one matching the resolved cxx crate version.
# The smoke test stands in for cargo here and supplies it explicitly.
CXXBRIDGE_RUNTIME=$(ls -dt "${TARGET_DIR}/${BEEKEEPER_PROFILE}"/build/cxx-*/out/libcxxbridge1.a 2>/dev/null | head -1)
if [ -z "${CXXBRIDGE_RUNTIME}" ]; then
  echo "libcxxbridge1.a not found under ${TARGET_DIR}/${BEEKEEPER_PROFILE}/build" >&2
  exit 1
fi

# The remaining undefined cxxbridge symbols are implemented in Rust (the
# extern "Rust" half of the bridge plus cxx's Vec/String intrinsics); the
# beekeeper rlib provides them in real builds, the smoke test stubs them —
# they are referenced by the link but never called.
RUST_SIDE="${WORK_DIR}/rust_side_symbols.txt"
{ nm -u "${BUNDLE}"; nm -u "${CXXBRIDGE_RUNTIME}"; } \
  | awk '{print $NF}' | grep -E 'cxxbridge1\$' | sort -u > "${RUST_SIDE}"

# Smoke test: reference every bridge symbol from a C++ program and link it
# against the bundle plus system libs only — no Boost, no OpenSSL, no
# beekeeper checkout. Running it executes the bundled C++ static initializers
# (.init_array). The try/catch forces this TU to define the exception
# personality reference (DW.ref.__gxx_personality_v0) the bundle's unwind
# tables need under PIE.
SMOKE_SRC="${WORK_DIR}/smoke_test.cc"
SMOKE_BIN="${WORK_DIR}/smoke_test"
{
  awk '{printf "extern \"C\" char sym_%d __asm__(\"%s\");\n", NR, $0}' "${EXPORTS}"
  awk '{printf "extern \"C\" void stub_%d() __asm__(\"%s\"); extern \"C\" void stub_%d() {}\n", NR, $0, NR}' "${RUST_SIDE}"
  echo "static const void* const refs[] = {"
  awk '{printf "    &sym_%d,\n", NR}' "${EXPORTS}"
  echo "};"
  echo "int main() { try { if (refs[0] == nullptr) throw 1; } catch (...) { return 1; } return 0; }"
} > "${SMOKE_SRC}"

"${CXX_BIN}" "${SMOKE_SRC}" "${ARTIFACT}" "${CXXBRIDGE_RUNTIME}" \
  -lpthread -ldl -o "${SMOKE_BIN}"
"${SMOKE_BIN}"

if ldd "${SMOKE_BIN}" | grep -qEi 'boost|libssl|libcrypto'; then
  echo "Smoke test binary dynamically links Boost/OpenSSL:" >&2
  ldd "${SMOKE_BIN}" | grep -Ei 'boost|libssl|libcrypto' >&2
  exit 1
fi

# Install the bundle into the crate and package it. A .crate file is what
# `cargo package` produces (a gzipped source tarball); crates.io enforces its
# 10 MB upload limit on exactly this file. The published crate carries the
# bundle at lib/libbeekeeper_native.a and build.rs links it whenever
# BEEKEEPER_FROM_SOURCE and the orchestrated-mode env vars are absent — i.e.
# on every consumer machine. The `cargo package` verify build compiles the
# packaged copy (which has no C++ sources), so it exercises exactly that
# prebuilt path.
CRATE_LIB_DIR="${SCRIPT_DIR}/lib"
mkdir -p "${CRATE_LIB_DIR}"
cp "${ARTIFACT}" "${CRATE_LIB_DIR}/libbeekeeper_native.a"

echo "Installed $(du -h "${CRATE_LIB_DIR}/libbeekeeper_native.a" | cut -f1) bundle at ${CRATE_LIB_DIR}/libbeekeeper_native.a"
