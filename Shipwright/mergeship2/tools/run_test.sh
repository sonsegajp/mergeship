#!/bin/bash
# MergeShip2: run one harness script against the built soh.exe, convert screenshots to PNG.
# Usage: run_test.sh <testdir> [timeout-seconds]
D=$(cd "$1" && pwd)
W=$(cd "$1" && pwd -W)
TO=${2:-240}
rm -f "$D"/*.bmp "$D"/*.png "$D"/trace.log
SRC=${MS_EXE_DIR:-/c/Users/hyper/Mergeship2/Shipwright/x64/Debug}
if [ -n "$MS_EXE_DIR" ]; then
    # Run from a private copy of the build so the test never shares a log or config with a game the user is playing.
    RUN=/c/Users/hyper/Mergeship2/build/testrun
    mkdir -p $RUN
    cp -f "$SRC/${MS_EXE:-soh.exe}" $RUN/
    for f in mm.o2r oot.o2r soh.o2r mergeship2.o2r; do
        [ "$SRC/$f" -nt "$RUN/$f" ] && cp -f "$SRC/$f" $RUN/
    done
    # Always the user's current settings: a test run may have saved its own CVars into the private copy.
    cp -f "$SRC/shipofharkinian.json" $RUN/
    for d in assets Randomizer mm-extractor; do
        [ -d "$RUN/$d" ] || cp -r "$SRC/$d" $RUN/
    done
    cd $RUN
else
    cd $SRC
fi
rm -rf logs
MERGESHIP_TEST_SCRIPT="$W/script.txt" timeout $TO ./${MS_EXE:-soh.exe} > /dev/null 2>&1
echo "exit=$?"
grep -h -A40 "CrashHandler" logs/*.log > "$D/crash.txt" 2>/dev/null || rm -f "$D/crash.txt"
python - "$D" <<'PY'
import sys,glob,os
from PIL import Image
for b in glob.glob(os.path.join(sys.argv[1],'*.bmp')):
    Image.open(b).save(b[:-4]+'.png'); os.remove(b)
PY
ls "$D"
