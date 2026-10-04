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

## The test runtime image (`runtime/`)

The suites run in a container with `--network none` and your uid. The image is CI's
`emsdk:5.0.2-4` (the npm_projects template's `EMSCRIPTEN_IMAGE_TAG`: emscripten, Node
24.21.0, pnpm 10.0.0) plus the chromium build the lockfile's Playwright wants, a pnpm
store filled from `beekeeper_wasm/pnpm-lock.yaml`, Ubuntu's g++/Boost/OpenSSL for the
native build, and Python 3.12 with `python/poetry.lock` (dev and static-analysis
groups) in `/opt/py`.

When `beekeeper_wasm/pnpm-lock.yaml`, the npm-common-config submodule's
`pnpm-workspace.yaml`/`.npmrc`, `packageManager`, `python/pyproject.toml`, either
`poetry.lock` or `runtime/Dockerfile` change, rebuild and re-pin **in the same commit**:

```bash
.aidev/runtime/build.sh --push   # registry digest if aidev-<input hash> exists, else build + push
# put the printed repo@sha256:<digest> into project.yaml environment.image
```

Run a suite by hand the same way AIDEV does:

```bash
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v "$PWD":/work -w /work <environment.image> .aidev/run-checks.sh quick wasm build test
```
