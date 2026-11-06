#!/usr/bin/env bash
# Build the test runtime of .aidev/runtime/Dockerfile locally, as AIDEV builds it, and
# print its tag, for running a suite by hand (see .aidev/README.md). AIDEV builds its
# own copy on the followed `environment.base` of .aidev/project.yaml whenever the base
# tag, the Dockerfile or an `environment.inputs` file moves, so nothing printed here is
# ever pinned in project.yaml.
#
#   .aidev/runtime/build.sh           # build on the digest the followed tag names now
#   .aidev/runtime/build.sh <tag>     # the same, tagged <tag> (default beekeeper-aidev-tests:local)
set -euo pipefail

cd "$(dirname "$0")/../.."

TAG="${1:-beekeeper-aidev-tests:local}"
BASE_IMAGE="$(sed -n 's/^ *base: *{image: *"\([^"]*\)".*/\1/p' .aidev/project.yaml)"
CCC_IMAGE="$(scripts/ccc_emscripten_image.sh)"
[ "$CCC_IMAGE" = "${BASE_IMAGE:?no environment.base image in .aidev/project.yaml}" ] \
    || echo "building on $BASE_IMAGE; ccc's npm_projects template names $CCC_IMAGE" >&2
BASE_DIGEST="$(docker buildx imagetools inspect "$BASE_IMAGE" --format '{{json .Manifest}}' \
    | grep -o '"digest": *"sha256:[0-9a-f]*"' | head -1 | grep -o 'sha256:[0-9a-f]*')"

[ -f programs/beekeeper/beekeeper_wasm/npm-common-config/pnpm-config/pnpm-workspace.yaml ] \
    || { echo "the npm-common-config submodule is not checked out" >&2; exit 1; }

docker build --build-arg BASE_IMAGE="${BASE_IMAGE%:*}@${BASE_DIGEST:?cannot resolve $BASE_IMAGE}" \
    -f .aidev/runtime/Dockerfile -t "$TAG" . >&2
echo "$TAG"
