# shellcheck shell=bash
# Sourced by .aidev/run-checks.sh from the repository root, before the native and
# WASM builds: picks the sccache backend both compile through.
#
# AIDEV hands every suite its site's cache (aidev docs/operations/sccache-cache.md):
# AIDEV_SCCACHE_WEBDAV_ENDPOINT where a site WebDAV cache serves the host (Jefferson,
# Poland), else AIDEV_SCCACHE_REDIS (CI's Redis, Alleghany). The suite container
# reaches it through the slot stack's relay (.aidev/sccache-relay.compose.yml) as
# http://sccache:8090/ or redis://sccache:6379, or at the handed address itself on a
# network with a route (a run by hand). The first that answers is used:
#   * WebDAV, when GET /healthz answers and sccache is 0.10 or later: 0.8 stats a key
#     with PROPFIND before reading it, the site caches answer every PROPFIND as an
#     empty collection, and 0.8 then reads every entry as empty (a cache error on
#     every compile, never a hit);
#   * Redis, when a PING answers within _CC_REDIS_MAX_RTT_MS (as wax's
#     compiler-cache.sh: from another site a hit is slower than compiling).
# Otherwise, or when the sccache server does not start, the builds get no launcher
# and compile cold; a missing cache never fails a build.
#
# Keys hash the compiler, its flags and the preprocessed source, which carry absolute
# paths: AIDEV mounts every slot's checkout at /work, so runs on any host share them.
#
# Sets cc_launcher ("sccache" or empty), cc_backend (webdav, redis or none),
# cc_endpoint and cc_note. compiler_cache_record STEP appends the step's sccache
# counts to $out/sccache.tsv; compiler_cache_stop stops the server.

_CC_RELAY=sccache
_CC_REDIS_MAX_RTT_MS=10
_CC_WEBDAV_MIN_VERSION=0.10

_cc_webdav_answers() {  # URL
    curl -sf -m 3 -o /dev/null "${1%/}/healthz"
}

# The inner bash's arguments and variables, not this shell's.
# shellcheck disable=SC2016
_cc_redis_rtt_ms() {  # HOST PORT: the round trip of a second PING, in ms
    timeout 3 bash -c '
        exec 3<>"/dev/tcp/$1/$2" || exit 1
        printf "PING\r\n" >&3 && read -r _ <&3 || exit 1
        t0=$EPOCHREALTIME
        printf "PING\r\n" >&3 && read -r reply <&3 || exit 1
        t1=$EPOCHREALTIME
        [[ "$reply" == +PONG* ]] || exit 1
        echo $(( (${t1//[.,]/} - ${t0//[.,]/}) / 1000 ))' _ "$1" "$2" 2>/dev/null
}

_cc_sccache_version() {
    sccache --version 2>/dev/null | awk '{ print $2 }'
}

_cc_select_webdav() {
    local direct="$AIDEV_SCCACHE_WEBDAV_ENDPOINT" rest path="" version url
    rest="${direct#*://}"
    [[ "$rest" == */* ]] && path="${rest#*/}"
    version="$(_cc_sccache_version)"
    if [ "$(printf '%s\n' "$_CC_WEBDAV_MIN_VERSION" "$version" | sort -V | head -1)" != "$_CC_WEBDAV_MIN_VERSION" ]; then
        cc_note="sccache ${version:-missing} cannot read the WebDAV cache ($direct), $_CC_WEBDAV_MIN_VERSION or later can"
        return 1
    fi
    for url in "http://$_CC_RELAY:8090/$path" "$direct"; do
        if _cc_webdav_answers "$url"; then
            export SCCACHE_WEBDAV_ENDPOINT="$url" SCCACHE_WEBDAV_KEY_PREFIX=beekeeper
            cc_backend=webdav cc_endpoint="$url"
            return 0
        fi
    done
    cc_note="the WebDAV cache $direct answers neither through the relay nor directly"
    return 1
}

_cc_select_redis() {
    local url hostport rtt
    for url in "redis://$_CC_RELAY:6379" "$AIDEV_SCCACHE_REDIS"; do
        hostport="${url#redis://}"; hostport="${hostport%%/*}"; hostport="${hostport##*@}"
        rtt="$(_cc_redis_rtt_ms "${hostport%:*}" "${hostport##*:}")" || continue
        if [ "$rtt" -gt "$_CC_REDIS_MAX_RTT_MS" ]; then
            cc_note="the Redis cache $url answers in ${rtt} ms, over ${_CC_REDIS_MAX_RTT_MS} ms"
            return 1
        fi
        export SCCACHE_REDIS_ENDPOINT="$url"
        cc_backend=redis cc_endpoint="$url"
        return 0
    done
    cc_note="the Redis cache $AIDEV_SCCACHE_REDIS answers neither through the relay nor directly"
    return 1
}

compiler_cache_select() {
    cc_launcher="" cc_backend=none cc_endpoint="" cc_note="AIDEV handed no sccache cache"
    if [ -n "${AIDEV_SCCACHE_WEBDAV_ENDPOINT:-}" ]; then
        _cc_select_webdav || return 0
    elif [ -n "${AIDEV_SCCACHE_REDIS:-}" ]; then
        _cc_select_redis || return 0
    else
        return 0
    fi
    # The server must outlive the Playwright suite between the WASM and native builds;
    # a server that dies later costs a cold compile, not a failed one.
    export SCCACHE_IDLE_TIMEOUT=0 SCCACHE_IGNORE_SERVER_IO_ERROR=1
    if ! sccache --start-server >&2; then
        cc_note="sccache did not start with the $cc_backend cache $cc_endpoint"
        cc_backend=none cc_endpoint=""
        return 0
    fi
    cc_launcher=sccache cc_note=""
}

# The step's counts since the last record (its build), as a row of $out/sccache.tsv,
# and sccache's own report in the step's log. $out is run-checks.sh's.
# shellcheck disable=SC2154
compiler_cache_record() {  # STEP
    local tsv="$out/sccache.tsv" counts=""
    [ -s "$tsv" ] || printf 'step\tbackend\tendpoint\tcompile_requests\thits\tmisses\tcache_errors\twrite_errors\tnote\n' > "$tsv"
    if [ -n "$cc_launcher" ]; then
        sccache --show-stats || true
        counts="$(sccache --show-stats --stats-format=json 2>/dev/null | python3 -c '
import json, sys
s = json.load(sys.stdin)["stats"]
total = lambda k: sum(s[k]["counts"].values())
print(s["compile_requests"], total("cache_hits"), total("cache_misses"), total("cache_errors"), s["cache_write_errors"], sep="\t")')" || counts=""
        sccache --zero-stats > /dev/null || true
    fi
    [ -n "$counts" ] || counts=$'\t\t\t\t'
    printf '%s\t%s\t%s\t%s\t%s\n' "$1" "$cc_backend" "$cc_endpoint" "$counts" "$cc_note" >> "$tsv"
}

compiler_cache_stop() {
    [ -z "$cc_launcher" ] || sccache --stop-server > /dev/null 2>&1 || true
}
