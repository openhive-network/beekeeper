# Sourced by .aidev/run-checks.sh from the TS package directory
# (programs/beekeeper/beekeeper_wasm): make node_modules match its pnpm-lock.yaml,
# offline, from the image's store. A marker records the lockfile and Node it was
# installed for; it is written only after an install that succeeded, and an
# install whose tools don't resolve is redone.
lock_id="$(sha256sum pnpm-lock.yaml | cut -d' ' -f1) $(node --version)"
marker=node_modules/.aidev-pnpm-lock
if [ "$(cat "$marker" 2>/dev/null)" != "$lock_id" ] || [ ! -x node_modules/.bin/tsc ] || [ ! -x node_modules/.bin/playwright ]; then
    echo "node_modules is not current for pnpm-lock.yaml: pnpm install --offline" >&2
    pnpm install --offline --frozen-lockfile < /dev/null || return 1
    [ -x node_modules/.bin/tsc ] && [ -x node_modules/.bin/playwright ] || { echo "pnpm install left no tsc/playwright" >&2; return 1; }
    printf '%s\n' "$lock_id" > "$marker.tmp" && mv "$marker.tmp" "$marker"
fi
