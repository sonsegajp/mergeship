#!/bin/bash
# MergeShip2: run one harness test on the final exe (mergeship.exe, staged in build/latest), recording every frame.
#
#   tools/run_final.sh <testdir> [timeout-seconds] [soh|2ship]
#
# <testdir> is build/tests/<name> (or a path) holding script.txt. Tests named mm_* run 2Ship's harness
# (MERGESHIP_TEST_SCRIPT_MM, first engine 2ship); every other test runs SoH's (MERGESHIP_TEST_SCRIPT_SOH).
# The run happens in a private copy of the staging directory (build/finalrun) with the staged settings and no saves,
# so a test never touches the user's game, config or files.
#
# Output in <testdir>: frames.mp4 (every frame the host ran, 60 fps, so video frame N is host frame N+1),
# frames.mp4.frames.csv (video frame -> host frame, engine), sheets/ (contact sheets, every 10th frame, 6x5 per
# sheet = 5 s each), the harness's trace and screenshots (PNG), mergeship-host.log, crash.txt on a crash.
ROOT=/c/Users/hyper/Mergeship2
STAGE=${MS_STAGE:-$ROOT/build/latest}
RUN=${MS_RUN:-$ROOT/build/finalrun}
FFMPEG=${MS_FFMPEG:-ffmpeg}

T=$1
[ -d "$T" ] || T=$ROOT/build/tests/$1
D=$(cd "$T" && pwd)
W=$(cd "$T" && pwd -W)
NAME=$(basename "$D")
TO=${2:-240}
ENGINE=$3
if [ -z "$ENGINE" ]; then
    if [ -f "$T/engine" ]; then ENGINE=$(cat "$T/engine"); else
    case "$NAME" in mm_adult_*|mm_items_*|mm_ooteq_*|mm_songs_*) ENGINE=2ship ;; *) ENGINE=soh ;; esac
    fi
fi

rm -rf "$D"/*.bmp "$D"/*.png "$D"/trace*.log "$D"/frames.mp4* "$D"/sheets "$D"/crash.txt "$D"/mergeship-host.log

mkdir -p $RUN
for f in mergeship.exe mergeship.pdb libultraship.dll libultraship.pdb SDL2.dll soh.dll soh.pdb 2ship.dll 2ship.pdb \
         oot.o2r soh.o2r mergeship2.o2r mm.o2r 2ship.o2r gamecontrollerdb.txt; do
    [ -f "$STAGE/$f" ] && [ "$STAGE/$f" -nt "$RUN/$f" ] && cp -f "$STAGE/$f" $RUN/
done
for d in assets randomizer presets; do
    [ -d "$STAGE/$d" ] && [ ! -d "$RUN/$d" ] && cp -r "$STAGE/$d" $RUN/
done
# The staged settings every time, and no saves: a run must not depend on what an earlier one left behind.
for f in shipofharkinian.json 2ship2harkinian.json imgui.ini; do
    [ -f "$STAGE/$f" ] && cp -f "$STAGE/$f" $RUN/
done
rm -rf $RUN/Save $RUN/saves $RUN/logs $RUN/mergeship-host.log
# Mods: only what MS_MODS (a folder of .o2r files) names, so a run never picks up an earlier run's mods.
rm -rf $RUN/mods
if [ -n "$MS_MODS" ]; then
    mkdir -p $RUN/mods && cp -f "$MS_MODS"/*.o2r $RUN/mods/
fi
cd $RUN
# With the displays asleep, DWM paces V-Sync'd presents at ~7.6 Hz (tools/test_host.ps1); the private copy runs
# without V-Sync. Recording paces the frames anyway, and the games step one frame per frame whatever the speed.
python - <<'PY'
import json, os
for name in ("shipofharkinian.json", "2ship2harkinian.json"):
    data = json.load(open(name)) if os.path.exists(name) else {}
    data.setdefault("CVars", {}).setdefault("gSettings", {})["VsyncEnabled"] = 0
    # Anchor: never the player's own room. The staged settings may be the user's play settings (their room, name and
    # anchor.hm64.org); a test run joined that room as them (2026-09-30). Off unless the test asks with MS_ANCHOR=1,
    # and then the local mock server (tools/anchor_mock_server.py) under a test name and room.
    anchor = data["CVars"].setdefault("gRemote", {}).setdefault("Anchor", {})
    if os.environ.get("MS_ANCHOR") == "1":
        anchor.update({"Enabled": 1, "Host": "127.0.0.1", "Port": int(os.environ.get("MS_ANCHOR_PORT", "43383")),
                       "RoomId": "mergeship-test", "Name": "TestLink", "TeamId": "default"})
    else:
        anchor["Enabled"] = 0
    # The user's settings may have the port menu open (copied mid-play): tests run with it closed (MS_MENU=1 keeps
    # what the staged settings say, for a test of the menu itself).
    if os.environ.get("MS_MENU") != "1":
        for key in ("gOpenWindows", "gWindows"):
            data["CVars"].setdefault(key, {})["Menu"] = 0
    # MS_CVARS_JSON='{"gTrackers": {...}}': settings merged into both games' CVars before the run (colours, layouts).
    def merge(dst, src):
        for k, v in src.items():
            if isinstance(v, dict) and isinstance(dst.get(k), dict):
                merge(dst[k], v)
            else:
                dst[k] = v
    if os.environ.get("MS_CVARS_JSON"):
        merge(data["CVars"], json.loads(os.environ["MS_CVARS_JSON"]))
    # MS_OPEN_WINDOWS=ItemTracker,...: windows open from the start (a window reads its setting only when it is made).
    for win in filter(None, os.environ.get("MS_OPEN_WINDOWS", "").split(",")):
        for key in ("gOpenWindows", "gWindows"):
            data["CVars"].setdefault(key, {})[win] = 1
    # A 1280x960 window (MS_WINDOW=WxH for another), so text and small models are readable in the recording.
    window = data.setdefault("Window", {})
    width, height = (int(v) for v in os.environ.get("MS_WINDOW", "1280x960").split("x"))
    # Off screen (the user's request, 2026-10-07: tests run muted and unseen); MS_VISIBLE=1 puts it on screen.
    visible = os.environ.get("MS_VISIBLE") == "1"
    window.update({"Width": width, "Height": height, "PositionX": 40 if visible else -4000,
                   "PositionY": 40 if visible else 40, "Fullscreen": {"Enabled": False}})
    json.dump(data, open(name, "w"), indent=4)
PY

unset MERGESHIP_TEST_SCRIPT MERGESHIP_TEST_SCRIPT_SOH MERGESHIP_TEST_SCRIPT_MM MERGESHIP_TEST_SCRIPT_2SHIP
if [ "$ENGINE" = "2ship" ]; then
    export MERGESHIP_TEST_SCRIPT_MM="$W/script.txt"
else
    export MERGESHIP_TEST_SCRIPT_SOH="$W/script.txt"
fi
echo "running $NAME on mergeship.exe (first engine $ENGINE, ${TO}s), recording every frame"
timeout $TO ./mergeship.exe --engine $ENGINE --muted --record "$W/frames.mp4" > /dev/null 2>&1 &
GAME=$!
# A crash leaves the game waiting on its crash dialog until the timeout: end it once the dump is written.
while kill -0 $GAME 2>/dev/null; do
    if ls logs/*crash.dmp > /dev/null 2>&1; then
        sleep 5
        taskkill //IM mergeship.exe //F > /dev/null 2>&1
        echo "crash dump written; game ended"
        break
    fi
    sleep 2
done
wait $GAME
echo "exit=$?"
# ffmpeg finishes the video once the game's pipe closes.
for i in $(seq 1 60); do
    tasklist 2>/dev/null | grep -qi "^ffmpeg.exe" || break
    sleep 1
done

cp -f mergeship-host.log "$D/" 2>/dev/null
mkdir -p "$D/logs" && cp -f logs/*.log "$D/logs/" 2>/dev/null
grep -h -A40 "CrashHandler" logs/*.log > "$D/crash.txt" 2>/dev/null
[ -s "$D/crash.txt" ] || rm -f "$D/crash.txt"
python - "$D" <<'PY'
import sys, glob, os
from PIL import Image
for b in glob.glob(os.path.join(sys.argv[1], '*.bmp')):
    Image.open(b).save(b[:-4] + '.png'); os.remove(b)
PY
if [ -f "$D/frames.mp4" ]; then
    bash $ROOT/tools/make_sheets.sh "$D"
    FRAMES=$(ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames -of csv=p=0 "$D/frames.mp4" 2>/dev/null | tr -dc 0-9)
    echo "recorded ${FRAMES:-?} frames; $(ls "$D/sheets" | wc -l) contact sheets"
else
    echo "NO RECORDING"
fi
ls "$D"
