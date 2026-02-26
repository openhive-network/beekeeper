#! /bin/bash

set -xeuo pipefail

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
PROJECT_DIR="${SCRIPTPATH}/.."

DIRECT_EXECUTION_DEFAULT=0
EXECUTION_PATH_DEFAULT="/src/"

# Check for usage inside dev container providing all tools (emscripten image)
if [ $# -eq 0 ]; then
  EXECUTOR=$(whoami)
  if [ "${EXECUTOR}" = "emscripten" ]; then
    DIRECT_EXECUTION_DEFAULT=1
    EXECUTION_PATH_DEFAULT="${PROJECT_DIR}"
  fi
fi

DIRECT_EXECUTION=${1:-${DIRECT_EXECUTION_DEFAULT}}
EXECUTION_PATH=${2:-"${EXECUTION_PATH_DEFAULT}"}

build() {
  BUILD_DIR="${EXECUTION_PATH}/programs/beekeeper/beekeeper_wasm/src/build"
  mkdir -vp "${BUILD_DIR}"
  cd "${BUILD_DIR}"

  #-DBoost_DEBUG=TRUE -DBoost_VERBOSE=TRUE -DCMAKE_STATIC_LIBRARY_SUFFIX=".a;.bc"
  cmake \
    -DBoost_NO_WARN_NEW_VERSIONS=1 \
    -DBoost_USE_STATIC_RUNTIME=ON \
    -DCMAKE_TOOLCHAIN_FILE=/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DCMAKE_BUILD_TYPE=Release -G "Ninja" \
    -S "${EXECUTION_PATH}/programs/beekeeper/beekeeper_wasm/" \
    -B "${BUILD_DIR}"
  ninja -v -j8 2>&1 | tee -i "${BUILD_DIR}/build.log"

  GLUE="${BUILD_DIR}/beekeeper_wasm.common.js"

  # Strip node: prefix from all module specifiers so bundlers (Parcel/Webpack) can
  # resolve them. Emscripten 5.x generates "node:fs" etc.; 4.x used plain names.
  # We keep createRequire + require() intact — Parcel ignores require() calls,
  # but statically resolves import(), so do NOT replace require with import here.
  sed -i 's#"node:module"#"module"#g' "${GLUE}"
  sed -i 's#"node:fs"#"fs"#g' "${GLUE}"
  sed -i 's#"node:path"#"path"#g' "${GLUE}"
  sed -i 's#"node:url"#"url"#g' "${GLUE}"
  sed -i 's#"node:crypto"#"crypto"#g' "${GLUE}"

  # Replace bare process.* with globalThis.process.* so Parcel does not try to
  # polyfill the "process" Node.js builtin in browser bundles.
  # Only match process not preceded by a dot (avoids double-replacing globalThis.process).
  sed -i 's#\([^.]\)process\.argv#\1globalThis.process.argv#g' "${GLUE}"
  sed -i 's#\([^.]\)process\.exitCode#\1globalThis.process.exitCode#g' "${GLUE}"
  # Handle process at start of line
  sed -i 's#^process\.argv#globalThis.process.argv#g' "${GLUE}"
  sed -i 's#^process\.exitCode#globalThis.process.exitCode#g' "${GLUE}"
}

if [ "${DIRECT_EXECUTION}" -eq 0 ]; then
  echo "Performing a docker run"
  docker run \
    -it --rm \
    -v "${PROJECT_DIR}/":"${EXECUTION_PATH}" \
    -u "$(id -u):$(id -g)" \
  registry.gitlab.syncad.com/hive/common-ci-configuration/emsdk:5.0.2-1 \
  /bin/bash /src/scripts/build_wasm_beekeeper.sh 1 "${EXECUTION_PATH}"
else
  echo "Performing a build..."
  cd "${EXECUTION_PATH}"
  build
fi
