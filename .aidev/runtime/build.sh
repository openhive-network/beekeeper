#!/usr/bin/env bash
# Build (and with --push, publish) the test runtime image of .aidev/runtime/Dockerfile
# and print the digest-pinned reference to put in .aidev/project.yaml's
# `environment.image`. See .aidev/README.md.
#
# The tag is `aidev-<hash>`, the hash of the image inputs (the Dockerfile,
# `packageManager` from package.json, pnpm-lock.yaml, pnpm-workspace.yaml,
# .npmrc); the registry cleanup policy keeps `aidev-*` tags. When
# <repository>:aidev-<hash> is already in the registry nothing is built: the script
# prints that image's digest.
#
#   .aidev/runtime/build.sh           # registry digest, or build locally and print the local tag
#   .aidev/runtime/build.sh --push    # registry digest, or build + push and print repo@sha256:<digest>
#   .aidev/runtime/build.sh --tag     # print the input-hash tag only
set -euo pipefail

cd "$(dirname "$0")/../.."

REPOSITORY="${AIDEV_RUNTIME_REPOSITORY:-registry.gitlab.syncad.com/hive/beekeeper/aidev-tests}"

PKG=programs/beekeeper/beekeeper_wasm
CFG=$PKG/npm-common-config/pnpm-config
PY=python
# `pnpm fetch` reads only the lockfile, the workspace file and .npmrc (both from the
# npm-common-config submodule); of package.json the image checks `packageManager`
# alone. poetry reads pyproject.toml and the lock files of python/ and local-tools.
input_hash() {
    [ -f "$CFG/pnpm-workspace.yaml" ] || { echo "the npm-common-config submodule is not checked out" >&2; exit 1; }
    {
        sha256sum .aidev/runtime/Dockerfile "$PKG/pnpm-lock.yaml" "$CFG/pnpm-workspace.yaml" "$CFG/.npmrc" \
            "$PY/pyproject.toml" "$PY/poetry.lock" "$PY/tests/local-tools/pyproject.toml" "$PY/tests/local-tools/poetry.lock"
        grep '"packageManager"' "$PKG/package.json"
    } | sha256sum | cut -c1-16
}
TAG="aidev-$(input_hash)"

case "${1:-}" in
    --tag) echo "$TAG"; exit 0 ;;
    ""|--push) ;;
    *) echo "usage: $0 [--push|--tag]" >&2; exit 2 ;;
esac

# Digest of <repository>:<tag> in the registry, empty when the tag doesn't exist.
registry_digest() {
    docker buildx imagetools inspect "$REPOSITORY:$TAG" --format '{{json .Manifest}}' 2>/dev/null \
        | grep -o '"digest": *"sha256:[0-9a-f]*"' | head -1 | grep -o 'sha256:[0-9a-f]*' || true
}

digest="$(registry_digest)"
if [ -n "$digest" ]; then
    echo "$REPOSITORY:$TAG exists in the registry; not building" >&2
    echo "$REPOSITORY@$digest"
    exit 0
fi
echo "$REPOSITORY:$TAG is not in the registry; building it" >&2

# Only the files the Dockerfile copies, symlinks resolved: the repository holds node_modules
# and build trees.
context="$(mktemp -d)"
trap 'rm -rf "$context"' EXIT
mkdir -p "$context/js" "$context/python/tests"
cp -L "$PKG/package.json" "$PKG/pnpm-lock.yaml" "$CFG/pnpm-workspace.yaml" "$CFG/.npmrc" "$context/js/"
cp "$PY/pyproject.toml" "$PY/poetry.lock" "$context/python/"
cp -r "$PY/tests/local-tools" "$context/python/tests/"
rm -rf "$context/python/tests/local-tools/"*.egg-info

# buildx so it works with any builder (CI's docker-container builder keeps no local
# image); no provenance, so the pushed digest is a plain image manifest.
build=(docker buildx build --pull --provenance=false -f .aidev/runtime/Dockerfile -t "$REPOSITORY:$TAG")
if [ "${1:-}" != "--push" ]; then
    "${build[@]}" --load "$context" >&2
    echo "$REPOSITORY:$TAG"
    exit 0
fi

"${build[@]}" --push --metadata-file "$context/metadata.json" "$context" >&2
digest="$(grep -o '"containerimage.digest": *"sha256:[0-9a-f]*"' "$context/metadata.json" | grep -o 'sha256:[0-9a-f]*')"
echo "$REPOSITORY@${digest:?no digest in buildx metadata}"
