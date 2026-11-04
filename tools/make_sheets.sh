#!/bin/bash
# MergeShip2: contact sheets of EVERY frame of a test recording: <testdir>/sheets/sheet_NNN.jpg, 100 frames per sheet
# (10x10, 192x144 each), every frame stamped with its video frame number (= harness frame - 1).
#   tools/make_sheets.sh <testdir>
D=$1
[ -f "$D/frames.mp4" ] || { echo "no recording in $D"; exit 1; }
rm -rf "$D/sheets"; mkdir -p "$D/sheets"
ffmpeg -hide_banner -loglevel error -i "$D/frames.mp4" \
    -vf "drawtext=fontfile='C\:/Windows/Fonts/consola.ttf':text='%{n}':x=8:y=8:fontsize=64:fontcolor=yellow:box=1:boxcolor=black@0.7,scale=192:144,tile=10x10" \
    -fps_mode vfr -q:v 3 "$D/sheets/sheet_%03d.jpg"
