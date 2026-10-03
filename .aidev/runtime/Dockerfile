# The image AIDEV runs this project's suites in (`environment.image` in
# .aidev/project.yaml). Suites run with `--network none` and the caller's uid, so
# everything they need is in the image:
#
# * the CI emsdk image the project's npm_projects template pins (EMSCRIPTEN_IMAGE_TAG
#   5.0.2-3): emscripten, cmake, ninja, Node 22.21.1 and pnpm 10.0.0 under nvm (as
#   CI's `nvm use "${NODEJS_VERSION}"`);
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
#   the subprocesses its tests start need.
#
# Build and pin it with .aidev/runtime/build.sh; see .aidev/README.md.
FROM registry.gitlab.syncad.com/hive/common-ci-configuration/emsdk:5.0.2-3@sha256:77d9f1994382bae6fdd01f44e8b1af3b0396651b1c4dbc8a50ed60d596bc73b3

USER root

ENV HOME=/tmp \
    PATH=/opt/py/bin:/home/emscripten/.nvm/versions/node/v22.21.1/bin:$PATH \
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

COPY js/ /tmp/pnpm-fetch/
RUN cd /tmp/pnpm-fetch \
    && test "$(node --version)" = v22.21.1 && test "$(pnpm --version)" = 10.0.0 \
    && pnpm fetch \
    && pnpm install --offline --frozen-lockfile --ignore-scripts \
    && pnpm exec playwright install chromium \
    && cd / && rm -rf /tmp/pnpm-fetch \
    && chmod -R a+rwX /opt/pnpm-store /opt/pw-browsers

COPY python/ /work/python/
RUN python3 -m venv --without-pip /opt/poetry \
    && pip3 --python /opt/poetry/bin/python install --no-cache-dir poetry==2.1.3 \
    && python3 -m venv --without-pip /opt/py \
    && cd /work/python \
    && POETRY_CACHE_DIR=/tmp/poetry-cache /opt/poetry/bin/poetry install --no-root --no-interaction \
         --with dev,static-analysis \
    && /opt/py/bin/ruff --version && /opt/py/bin/mypy --version \
    && echo /work/python > /opt/py/lib/python3.12/site-packages/beekeepy-workspace.pth \
    && rm -rf /work /tmp/poetry-cache /opt/poetry \
    && chmod -R a+rwX /opt/py /emsdk/upstream/emscripten/cache

USER emscripten
