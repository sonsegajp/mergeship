#!/bin/bash
# MergeShip2: build soh.exe and/or run harness tests under one machine-wide lock, so parallel
# agents never link while another runs the game (LNK1168) or run two games at once.
#
# Usage:
#   tools/locked_build_test.sh build                 incremental Debug build of soh
#   tools/locked_build_test.sh test <testdir> [secs] run one harness test (build/tests/<testdir>)
#   tools/locked_build_test.sh both <testdir> [secs] build, then test if the build succeeded
#   tools/locked_build_test.sh configure             re-run CMake configure (after adding .c/.cpp files)
#
# The build log goes to build/logs/soh-build-<pid>.log; errors are printed.
ROOT=/c/Users/hyper/Mergeship2
LOCK=$ROOT/build/.buildlock
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"

acquire() {
    local waited=0
    until mkdir "$LOCK" 2>/dev/null; do
        # Stale only if the holder is gone: a numeric owner pid that no longer exists, or (for a lock without a
        # pid, e.g. the coordinator's "user playing") no refresh for 20 minutes. A live holder refreshes the
        # lock every 30 s (below), so a long build is never mistaken for a dead one.
        local owner pid age
        owner=$(cat "$LOCK/owner" 2>/dev/null)
        pid=${owner%% *}
        age=$(( $(date +%s) - $(stat -c %Y "$LOCK" 2>/dev/null || date +%s) ))
        if [[ "$pid" =~ ^[0-9]+$ ]]; then
            if ! ps -p "$pid" >/dev/null 2>&1 && [ $age -gt 60 ]; then rm -rf "$LOCK"; continue; fi
        elif [ $age -gt 1200 ]; then
            rm -rf "$LOCK"; continue
        fi
        if [ $((waited % 60)) -eq 0 ]; then echo "waiting for build/test lock (held by $owner)"; fi
        sleep 5
        waited=$((waited + 5))
    done
    echo "$$ $1" > "$LOCK/owner"
    ( while [ -d "$LOCK" ] && ps -p $$ >/dev/null 2>&1; do touch "$LOCK" 2>/dev/null; sleep 30; done ) &
    trap 'rm -rf "$LOCK"' EXIT
}

do_build() {
    local log=$ROOT/build/logs/soh-build-$$.log
    mkdir -p $ROOT/build/logs
    "$CMAKE" --build $ROOT/Shipwright/build/x64 --config Debug --target soh --parallel 8 > "$log" 2>&1
    local rc=$?
    echo "build exit=$rc (log: $log)"
    grep -E "error C|error LNK|fatal error" "$log" | sed 's/\[C:.*//' | sort -u | head -40
    return $rc
}

do_test() {
    bash $ROOT/build/tests/run_test.sh "$ROOT/build/tests/$1" "${2:-240}"
}

case "$1" in
    build) acquire build; do_build ;;
    test) acquire "test $2"; do_test "$2" "$3" ;;
    both) acquire "both $2"; do_build && do_test "$2" "$3" ;;
    configure) acquire configure; "$CMAKE" $ROOT/Shipwright/build/x64 > $ROOT/build/logs/configure-$$.log 2>&1; echo "configure exit=$?" ;;
    *) echo "usage: $0 build | test <testdir> [secs] | both <testdir> [secs] | configure"; exit 2 ;;
esac
