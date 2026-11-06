# beekeeper under AIDEV

AIDEV verifies changes through the slots in `project.yaml`, integrates them into
`aidev/integration`, and people merge that into `master` through merge requests.
GitLab CI doesn't run for AIDEV branches (`ai/*`, `session/*`, pushes to
`aidev/integration`); see `.gitlab-ci.yml` `workflow:`.

## Suites

`.aidev/run-checks.sh <suite> <step>...` runs the named steps and writes
`test-results/<suite>/junit.xml` (one case per step, its log tail as the failure body),
plus one junit per test runner: `playwright-junit.xml`, `native-junit.xml`,
`beekeepy-junit.xml`. The script needs both submodules checked out
(`git submodule update --init --recursive`).

| Step | What | CI job it mirrors |
|---|---|---|
| `wasm` | `scripts/build_wasm_beekeeper.sh` (emscripten) into `beekeeper_wasm/src/build` | beekeeper_wasm_build |
| `build` | TS package: tsc, rollup, terser, size-limit | beekeeper_wasm_build |
| `typecheck` | `tsc --noEmit` (after `wasm`) | — |
| `test` | Playwright suite, chromium (after `wasm`, `build`) | beekeeper_wasm_test |
| `native` | cmake + ninja into `build/` (Release, tests on) | beekeeper_native_build |
| `native-test` | `build/tests/unit/beekeeper_test` (Boost.Test) | beekeeper_native_test |
| `beekeepy` | pytest `python/tests/beekeepy_test` against the native daemon | run_beekeepy_tests (3.12) |
| `py-format` / `py-lint` / `py-types` | ruff format --check / ruff check / mypy | beekeepy_formatting_with_ruff_check, pre-commit's ruff, beekeepy_type_check_with_mypy |

| Slot | Steps |
|---|---|
| quick | wasm, build, test, py-format, py-lint, py-types |
| full, canary | everything except typecheck |
| baseline | wasm, build, test, native, native-test, beekeepy |
| static | wasm, typecheck, py-* |
| coverage | native, native-test, beekeepy (no coverage is measured) |
| system | wasm, build, test, native, beekeepy |

Not covered: `run_examples_beekeeper_wasm` and `run_compatibility_beekeeper_wasm`
(they install the packed tarball and their own dependencies from the network), the
three beekeepy communicator test files that need a live Hive node (CI passes
`--hived-http-endpoint=https://api.hive.blog`), the python 3.14 leg,
`beekeeper_native_check_static_linking` (the image links OpenSSL dynamically), the
pre-commit hooks other than ruff/mypy, and packaging/publishing.

## The compiler cache

`wasm` and `native` compile through sccache (`CMAKE_C_COMPILER_LAUNCHER` /
`CMAKE_CXX_COMPILER_LAUNCHER`; sccache handles emcc too) with the site's cache AIDEV
hands every slot (aidev `docs/operations/sccache-cache.md`): a WebDAV cache in Jefferson
and Poland (`AIDEV_SCCACHE_WEBDAV_ENDPOINT`), CI's Redis in Alleghany
(`AIDEV_SCCACHE_REDIS`). The slot's stack (`sandbox:` in `project.yaml`) is one relay,
`sccache-relay.compose.yml`: the suite container sits on the stack's internal network,
reaching the relay and nothing else, and the relay forwards `sccache:8090` to the
WebDAV cache and `sccache:6379` to the Redis.

`compiler-cache.sh` picks the backend once per run: the WebDAV cache when its `/healthz`
answers and it answers sccache's reads (a `PROPFIND` of a missing key is a 404, of
sccache's stored `.sccache_check` key a non-collection with its size: what sccache before
0.10 needs to read anything), else the Redis when a PING answers within 10 ms, else none.
Through the relay first, then at the handed address (a run by hand on a network with a
route). With no backend, or when the sccache server will not start, the builds get no
launcher and compile cold; an unreachable cache never fails a slot. `SCCACHE_BASEDIRS` is
the checkout, so the cache keys do not carry its absolute path: a run by hand from any
directory shares the keys of the slots, which mount it at `/work`.

`test-results/<suite>/sccache.tsv` has one row per build step: the backend and endpoint,
sccache's compile requests, hits, misses, cache errors and write errors for that build,
or why no cache was used. The step's log ends with `sccache --show-stats`.

## The test runtime image (`runtime/`)

The suites run in a container with your uid, on the stack's internal network (see *The
compiler cache*) or with `--network none` by hand. The image is CI's
emsdk image at the tag `runtime/Dockerfile`'s `BASE_IMAGE` names (emscripten, sccache
0.19.0 and ccc's Node and pnpm; `runtime/build.sh` builds on its current digest, and notes
when ccc's npm_projects template still names an older tag as `EMSCRIPTEN_IMAGE`) plus the chromium build the lockfile's Playwright wants, a pnpm
store filled from `beekeeper_wasm/pnpm-lock.yaml`, Ubuntu's g++/Boost/OpenSSL for the
native build, and Python 3.12 with `python/poetry.lock` (dev and static-analysis
groups) in `/opt/py`, plus pytest, pytest-asyncio and pytest-xdist at the versions of
ccc's python-test-tools image (its pip constraints, which `runtime/build.sh` reads as
CI's `.beekeepy_test_tools` does).

When `beekeeper_wasm/pnpm-lock.yaml`, the npm-common-config submodule's
`pnpm-workspace.yaml`/`.npmrc`, `packageManager`, `python/pyproject.toml`, either
`poetry.lock` or `runtime/Dockerfile` (its emsdk tag) change, or ccc republishes that
emsdk tag or moves its python-test-tools versions, rebuild and re-pin **in the same commit**:

```bash
.aidev/runtime/build.sh --push   # registry digest if aidev-<input hash> exists, else build + push
# put the printed repo@sha256:<digest> into project.yaml environment.image
```

Run a suite by hand the same way AIDEV does:

```bash
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v "$PWD":/work -w /work <environment.image> .aidev/run-checks.sh quick wasm build test
```
