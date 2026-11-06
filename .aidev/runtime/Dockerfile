# The image AIDEV runs this project's suites in. AIDEV builds it itself: BASE_IMAGE is
# the digest the followed `environment.base` tag of .aidev/project.yaml names now, and a
# change to this file or to an `environment.inputs` file rebuilds it on the next run;
# nothing is re-pinned. The build context is the checkout, narrowed by
# Dockerfile.dockerignore to the files copied below. Suites run with the caller's uid on
# the stack's internal network or `--network none`, so everything they need is in the
# image:
#
# * ccc's CI emsdk image (the followed base): emscripten, cmake, ninja, sccache, and
#   ccc's Node and pnpm under nvm (as CI's `nvm use "${NODEJS_VERSION}"`). This file
#   pins no Node version. The base tag is ahead of the one ccc's npm_projects template
#   names (`EMSCRIPTEN_IMAGE`, scripts/ccc_emscripten_image.sh) for its sccache 0.19.0,
#   whose SCCACHE_BASEDIRS compiler-cache.sh sets; build.sh notes when the two differ;
# * the chromium build the lockfile's Playwright wants, in /opt/pw-browsers (CI's
#   beekeeper_wasm_test runs `playwright install chromium`; the emsdk image's own
#   pw-browsers carry a different build);
# * a pnpm store filled from programs/beekeeper/beekeeper_wasm/pnpm-lock.yaml, with
#   the catalog-bearing pnpm-workspace.yaml and .npmrc from the npm-common-config
#   submodule;
# * g++ and Boost/OpenSSL dev packages for the native build (cmake + ninja);
# * Python 3.12 with python/poetry.lock's main, dev and static-analysis groups in
#   /opt/py (ruff, mypy), installed by poetry 2.1.3 from a copy at /work/python so
#   the editable local-tools path dependency resolves to the mounted workspace, and
#   beekeepy itself (`--no-root`) is importable from /work/python through a .pth, as
#   the subprocesses its tests start need; pytest, pytest-asyncio and pytest-xdist at
#   ccc's versions: the pip constraints of the python-test-tools image (CI's
#   .beekeepy_test_tools), which ccc's script reads from the registry at build time.
#
# See .aidev/README.md.
ARG BASE_IMAGE
FROM ${BASE_IMAGE}
ARG PYTHON_TEST_TOOLS_IMAGE=registry.gitlab.syncad.com/hive/common-ci-configuration/python-test-tools:develop
ARG CCC_SCRIPTS=https://gitlab.syncad.com/hive/common-ci-configuration/-/raw/develop/scripts/python

USER root

ENV HOME=/tmp \
    PATH=/opt/py/bin:/home/emscripten/.nvm/versions/node/${NODEJS_VERSION}/bin:$PATH \
    VIRTUAL_ENV=/opt/py \
    npm_config_store_dir=/opt/pnpm-store \
    PLAYWRIGHT_BROWSERS_PATH=/opt/pw-browsers \
    HUSKY=0 \
    DO_NOT_TRACK=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    MYPY_CACHE_DIR=/tmp/.mypy_cache \
    RUFF_CACHE_DIR=/tmp/.ruff_cache

# The native daemon and its Boost.Test suite (CI's beekeeper_native_build/_test use
# ci-base-image, a manylinux image with static OpenSSL; here it is Ubuntu 24.04's
# Boost 1.83 and shared OpenSSL, so the static-linking check is not reproduced).
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
         g++ libboost-all-dev libssl-dev zlib1g-dev libbz2-dev libsnappy-dev libreadline-dev \
    && rm -rf /var/lib/apt/lists/*

COPY programs/beekeeper/beekeeper_wasm/package.json programs/beekeeper/beekeeper_wasm/pnpm-lock.yaml /tmp/pnpm-fetch/
COPY programs/beekeeper/beekeeper_wasm/npm-common-config/pnpm-config/pnpm-workspace.yaml \
     programs/beekeeper/beekeeper_wasm/npm-common-config/pnpm-config/.npmrc /tmp/pnpm-fetch/
RUN cd /tmp/pnpm-fetch \
    && test "$(node --version)" = "${NODEJS_VERSION}" && test "$(pnpm --version)" = 10.33.0 \
    && pnpm fetch \
    && pnpm install --offline --frozen-lockfile --ignore-scripts \
    && pnpm exec playwright install chromium \
    && cd / && rm -rf /tmp/pnpm-fetch \
    && chmod -R a+rwX /opt/pnpm-store /opt/pw-browsers

COPY python/pyproject.toml python/poetry.lock /work/python/
COPY python/tests/local-tools/ /work/python/tests/local-tools/
RUN mkdir /tmp/ccc \
    && for f in python-test-tools-constraints.py registry_auth.py; do \
         curl -fsSL "${CCC_SCRIPTS}/${f}" -o "/tmp/ccc/${f}"; \
       done \
    && python3 /tmp/ccc/python-test-tools-constraints.py "${PYTHON_TEST_TOOLS_IMAGE}" /tmp/python-test-tools-constraints.txt \
    && python3 -m venv --without-pip /opt/poetry \
    && pip3 --python /opt/poetry/bin/python install --no-cache-dir poetry==2.1.3 \
    && python3 -m venv --without-pip /opt/py \
    && cd /work/python \
    && POETRY_CACHE_DIR=/tmp/poetry-cache /opt/poetry/bin/poetry install --no-root --no-interaction \
         --with dev,static-analysis \
    && pip3 --python /opt/py/bin/python install --no-cache-dir -c /tmp/python-test-tools-constraints.txt \
         pytest pytest-asyncio pytest-xdist \
    && /opt/py/bin/ruff --version && /opt/py/bin/mypy --version \
    && echo /work/python > /opt/py/lib/python3.12/site-packages/beekeepy-workspace.pth \
    && rm -rf /work /tmp/poetry-cache /opt/poetry /tmp/ccc /tmp/python-test-tools-constraints.txt \
    && chmod -R a+rwX /opt/py /emsdk/upstream/emscripten/cache

USER emscripten
