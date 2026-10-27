#! /bin/bash
# Print the emsdk image common-ci-configuration's npm_projects template names
# (`EMSCRIPTEN_IMAGE`): the one CI's wasm jobs run, and the source of their Node.
# A set EMSCRIPTEN_IMAGE (as in CI, where the template exports it) is used as is.
set -euo pipefail

if [ -n "${EMSCRIPTEN_IMAGE:-}" ]; then
  echo "${EMSCRIPTEN_IMAGE}"
  exit 0
fi

CCC_TEMPLATE_URL="https://gitlab.syncad.com/hive/common-ci-configuration/-/raw/develop/templates/npm_projects.gitlab-ci.yml"
tag="$(curl -fsSL "${CCC_TEMPLATE_URL}" | sed -n 's/^ *EMSCRIPTEN_IMAGE_TAG: *"\{0,1\}\([^"]*\)"\{0,1\} *$/\1/p' | head -1)"
echo "registry.gitlab.syncad.com/hive/common-ci-configuration/emsdk:${tag:?no EMSCRIPTEN_IMAGE_TAG in ${CCC_TEMPLATE_URL}}"
