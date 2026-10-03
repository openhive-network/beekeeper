#!/usr/bin/env bash
# The checks AIDEV's verification slots run (.aidev/project.yaml), as one junit
# report per suite: each named step is a test case, its log the failure body.
#
#   .aidev/run-checks.sh <suite> <step>...
#
#   wasm       the WASM module: scripts/build_wasm_beekeeper.sh (emscripten,
#              cmake + ninja) into programs/beekeeper/beekeeper_wasm/src/build,
#              as CI's beekeeper_wasm_build does
#   build      the TS package (@hiveio/beekeeper): tsc, rollup, terser and the
#              size-limit budget, i.e. package.json `build` + `postbuild`
#              without its `prebuild` (husky + the wasm build, which is `wasm`)
#   typecheck  tsc --noEmit on the TS package (needs `wasm` for its .d.ts)
#   test       the Playwright suite (package.json `test`, CI's beekeeper_wasm_test);
#              every test its own case in $out/playwright-junit.xml. Needs `wasm`
#              and `build` first
#   native     the native daemon and its unit tests: cmake (Release, BUILD_TESTS=ON) +
#              ninja into build/, as CI's beekeeper_native_build
#   native-test  build/tests/unit/beekeeper_test (Boost.Test, CI's
#              beekeeper_native_test); its junit in $out/native-junit.xml
#   beekeepy   the beekeepy pytest suite against the native daemon (CI's
#              run_beekeepy_tests, python 3.12 leg). The communicator tests
#              that need a live Hive node (CI's --hived-http-endpoint=api.hive.blog)
#              are left out: suites run with no network. Its junit in
#              $out/beekeepy-junit.xml.
#              Needs `native` first
#   py-format  ruff format --check (CI's beekeepy_formatting_with_ruff_check)
#   py-lint    ruff check (the ruff hook of CI's beekeepy_pre_commit_checks)
#   py-types   mypy (CI's beekeepy_type_check_with_mypy)
set -uo pipefail
cd "$(dirname "$0")/.."
root="$PWD"
pkg="$root/programs/beekeeper/beekeeper_wasm"

suite="${1:?usage: $0 <suite> <step>...}"; shift
out="$root/test-results/$suite"
rm -rf "$out"; mkdir -p "$out"
cases="$out/cases.tsv"; : > "$cases"
source .aidev/junit-helpers.sh

# The TS package's workspace file and .npmrc are symlinks into the
# npm-common-config submodule; without it nothing below can install.
if [ ! -f "$pkg/npm-common-config/pnpm-config/pnpm-workspace.yaml" ]; then
    printf 'case\tsubmodules\tfail\t0\tsubmodule programs/beekeeper/beekeeper_wasm/npm-common-config is not checked out\n' >> "$cases"
    junit_write_cases "$out/junit.xml" "$suite" "$cases"
    exit 1
fi

needs_node=0
for s in "$@"; do case "$s" in build|typecheck|test) needs_node=1 ;; esac; done
if [ "$needs_node" -eq 1 ]; then
    # shellcheck source=pnpm-deps.sh
    if ! (cd "$pkg" && source "$root/.aidev/pnpm-deps.sh"); then
        printf 'case\tinstall\tfail\t0\tpnpm install --offline failed\n' >> "$cases"
        junit_write_cases "$out/junit.xml" "$suite" "$cases"
        exit 1
    fi
fi

status=0
step() {
    local name="$1"; shift
    local log="$out/$name.log" t0=$SECONDS rc=0
    echo "== $name" >&2
    ( cd "$root" && "$@" ) > "$log" 2>&1 < /dev/null || rc=$?   # a step's cd stays in its subshell
    if [ "$rc" -eq 0 ]; then
        printf 'case\t%s\tpass\t%s\t\n' "$name" "$((SECONDS - t0))" >> "$cases"
    else
        status=1; tail -40 "$log" >&2
        printf 'case\t%s\tfail\t%s\texit %s\t%s\n' "$name" "$((SECONDS - t0))" "$rc" "$log" >> "$cases"
    fi
}

wasm_build() {
    rm -rf "$pkg/src/build"
    ./scripts/build_wasm_beekeeper.sh 1 "$root"
}

ts_build() {
    cd "$pkg" || return 1
    [ -f src/build/beekeeper_wasm.common.js ] || { echo "src/build has no WASM module: run the wasm step first" >&2; return 1; }
    rm -rf dist
    # rollup embeds npm_package_version; `pnpm build` would set it from package.json.
    export npm_package_version
    npm_package_version="$(node -p "require('./package.json').version")"
    pnpm exec tsc && pnpm exec rollup -c && pnpm exec tsx ./npm-common-config/ts-common/terser.ts && pnpm exec size-limit
}

playwright_tests() {
    cd "$pkg" || return 1
    [ -f dist/bundle/web.js ] || { echo "dist/bundle has no build: run the build step first" >&2; return 1; }
    rm -f results.xml results.json
    local rc=0
    # package.json `test`, minus `pretest` (the browser is in the image).
    env -u CI pnpm exec playwright test --workers 1 --max-failures 1 --project=beekeeper_testsuite || rc=$?
    [ -s results.xml ] && cp results.xml "$out/playwright-junit.xml"
    return "$rc"
}

# As CI's beekeepy_* jobs: from the repository root, python/pyproject.toml's config,
# PACKAGES_TO_CHECK.
py_pkgs=(python/tests/ python/beekeepy/)

native_build() {
    cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON && ninja -C build -j"$(nproc)"
}

native_tests() {
    [ -x build/tests/unit/beekeeper_test ] || { echo "no build/tests/unit/beekeeper_test: run the native step first" >&2; return 1; }
    local junit="$out/native-junit.xml"
    rm -f "$junit"
    run_with_junit_fallback "$junit" native-test \
        build/tests/unit/beekeeper_test --log_level=test_suite --logger="HRF,test_suite,stdout:JUNIT,all,$junit"
}

# Their `hived_http_endpoint` fixture needs a live Hive API node.
live_api_tests=(
    tests/beekeepy_test/communicator/test_jsonrpc.py
    tests/beekeepy_test/communicator/test_rest_api.py
    tests/beekeepy_test/communicator/test_rest_api_definitions.py
)

beekeepy_tests() {
    [ -x build/programs/beekeeper/beekeeper/beekeeper ] || { echo "no native daemon in build/: run the native step first" >&2; return 1; }
    cp build/programs/beekeeper/beekeeper/beekeeper python/beekeepy/beekeeper
    local junit="$out/beekeepy-junit.xml"
    cd python || return 1
    run_with_junit_fallback "$junit" beekeepy \
        python3 -m pytest --asyncio-mode auto -n auto --durations 0 -p no:cacheprovider --junitxml="$junit" \
        "${live_api_tests[@]/#/--ignore=}" tests/beekeepy_test
}

for s in "$@"; do
    case "$s" in
        wasm) step wasm wasm_build ;;
        build) step build ts_build ;;
        typecheck) step typecheck bash -c 'cd "$1" && [ -f src/build/beekeeper_wasm.common.js ] && pnpm exec tsc --noEmit' _ "$pkg" ;;
        test) step test playwright_tests ;;
        native) step native native_build ;;
        native-test) step native-test native_tests ;;
        beekeepy) step beekeepy beekeepy_tests ;;
        py-format) step py-format ruff format --config python/pyproject.toml --check --diff "${py_pkgs[@]}" ;;
        py-lint) step py-lint ruff check --config python/pyproject.toml "${py_pkgs[@]}" ;;
        py-types) step py-types mypy --config-file python/pyproject.toml "${py_pkgs[@]}" ;;
        *) echo "unknown step: $s" >&2; exit 2 ;;
    esac
done
junit_write_cases "$out/junit.xml" "$suite" "$cases"
exit "$status"
