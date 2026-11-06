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
#   * WebDAV, when GET /healthz answers and the cache answers sccache's reads: sccache
#     before 0.10 stats a key with PROPFIND before reading it and reads nothing unless
#     a missing key is a 404 and a stored one a non-collection with its
#     getcontentlength (as hive's run-checks.sh probes). The probed stored key is the
#     one sccache writes when its server starts; a cache without it yet is taken on the
#     404 alone;
#   * Redis, when a PING answers within _CC_REDIS_MAX_RTT_MS (as wax's
#     compiler-cache.sh: from another site a hit is slower than compiling).
# Otherwise, or when the sccache server does not start, the builds get no launcher
# and compile cold; a missing cache never fails a build.
#
# Keys hash the compiler, its flags and the preprocessed source, which carry absolute
# paths: SCCACHE_BASEDIRS strips the checkout's (sccache 0.14 and later), so a run by
# hand from another directory shares the keys of AIDEV's slots, which mount it at /work.
#
# Sets cc_launcher ("sccache" or empty), cc_backend (webdav, redis or none),
# cc_endpoint and cc_note. compiler_cache_record STEP appends the step's sccache
# counts to $out/sccache.tsv; compiler_cache_stop stops the server.

_CC_RELAY=sccache
_CC_REDIS_MAX_RTT_MS=10
_CC_KEY_PREFIX=beekeeper

_cc_propfind() {  # URL: the body of a Depth 0 PROPFIND, its status code on the last line
    curl -s -m 3 -X PROPFIND -H 'Depth: 0' -w '\n%{http_code}' "$1" 2>/dev/null
}

# 0 when the cache answers sccache's reads, 1 when it does not answer, 2 when it answers
# but sccache could not read from it (cc_note says why).
_cc_webdav_answers() {  # URL
    local keys="${1%/}/$_CC_KEY_PREFIX" reply
    curl -sf -m 3 -o /dev/null "${1%/}/healthz" || return 1
    reply="$(_cc_propfind "$keys/.sccache-probe-missing")"
    if [ "${reply##*$'\n'}" != 404 ]; then
        cc_note="the WebDAV cache $1 answers PROPFIND of a missing key with ${reply##*$'\n'}, not 404, so sccache could not read from it"
        return 2
    fi
    reply="$(_cc_propfind "$keys/.sccache_check")"
    [ "${reply##*$'\n'}" = 404 ] && return 0
    [[ "$reply" == *getcontentlength* && "$reply" != *collection* ]] && return 0
    cc_note="the WebDAV cache $1 answers PROPFIND of a stored key without its size, so sccache could not read from it"
    return 2
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

# A cache that answers but fails the read probe is not tried again at its other address:
# the relay forwards to the same server.
_cc_select_webdav() {
    local direct="$AIDEV_SCCACHE_WEBDAV_ENDPOINT" rest path="" url rc
    rest="${direct#*://}"
    [[ "$rest" == */* ]] && path="${rest#*/}"
    for url in "http://$_CC_RELAY:8090/$path" "$direct"; do
        rc=0; _cc_webdav_answers "$url" || rc=$?
        if [ "$rc" -eq 0 ]; then
            export SCCACHE_WEBDAV_ENDPOINT="$url" SCCACHE_WEBDAV_KEY_PREFIX="$_CC_KEY_PREFIX"
            cc_backend=webdav cc_endpoint="$url"
            return 0
        fi
        [ "$rc" -eq 1 ] || return 1
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
    export SCCACHE_IDLE_TIMEOUT=0 SCCACHE_IGNORE_SERVER_IO_ERROR=1 SCCACHE_BASEDIRS="$PWD"
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
