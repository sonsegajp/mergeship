#!/bin/bash
# MergeShip2: every harness test in build/tests, one after another, on the final exe (tools/run_final.sh: mergeship.exe,
# every frame recorded), then the host's own door and switch tests (tools/test_host.ps1, also recorded).
#
#   tools/run_all_final.sh [name-filter-regex]
#
# Summary: build/tests/final_summary.tsv (test, engine, exit, frames recorded, harness frames, shots, crash).
ROOT=/c/Users/hyper/Mergeship2
SUM=$ROOT/build/tests/final_summary.tsv
FILTER=${1:-.}
[ -f $SUM ] || echo -e "test	exit	frames	harness_last	shots	crash" > $SUM
for d in $ROOT/build/tests/*/; do
    n=$(basename "$d")
    [ -f "$d/script.txt" ] || continue
    echo "$n" | grep -qE "$FILTER" || continue
    [ -n "$EXCLUDE" ] && echo "$n" | grep -qE "$EXCLUDE" && continue
    out=$(bash $ROOT/tools/run_final.sh "$n" ${MS_TEST_TIMEOUT:-900} 2>&1)
    ex=$(echo "$out" | grep -o "^exit=[0-9]*" | cut -d= -f2)
    fr=$(echo "$out" | grep -o "recorded [0-9]* frames" | tr -dc 0-9)
    last=$(grep -o "^\[[0-9]*\]" "$d/trace.log" 2>/dev/null | tail -1 | tr -dc 0-9)
    shots=$(ls "$d"/*.png 2>/dev/null | wc -l)
    crash=$([ -f "$d/crash.txt" ] && echo CRASH || echo -)
    echo -e "$n\t$ex\t$fr\t$last\t$shots\t$crash" | tee -a $SUM
done
