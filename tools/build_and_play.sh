#!/usr/bin/env bash
# Build (tools/build_host.ps1 -Fast) and, the moment soh.dll is built clean, hand it over: close the game running from
# build/latest (only that exe), copy soh.dll/soh.pdb in, open the game. The rest of the build script (assets, 2ship)
# keeps running in the background; it is not waited for.
#
#   bash tools/build_and_play.sh
set -u
cd "$(dirname "$0")/.."
LOG=build/gd_build.log
DLL=build/host-soh/Release/soh.dll
EXE='C:\Users\hyper\Mergeship2\build\latest\mergeship.exe'

before=$(stat -c %Y "$DLL" 2>/dev/null || echo 0)
powershell -ExecutionPolicy Bypass -File tools/build_host.ps1 -Fast > "$LOG" 2>&1 &
BUILD=$!

# wait for soh.dll to be relinked (or the build to end without it)
while kill -0 $BUILD 2>/dev/null; do
    now=$(stat -c %Y "$DLL" 2>/dev/null || echo 0)
    if [ "$now" != "$before" ] && grep -q "soh.vcxproj -> " "$LOG"; then
        break
    fi
    sleep 3
done
if grep -qE "error C|error LNK" "$LOG"; then
    echo "BUILD FAILED"
    grep -E "error C|error LNK|warning C" "$LOG" | sort -u | head -20
    exit 1
fi
now=$(stat -c %Y "$DLL" 2>/dev/null || echo 0)
if [ "$now" = "$before" ]; then
    echo "soh.dll not rebuilt"
    tail -5 "$LOG"
    exit 1
fi
sleep 2 # (the linker closes the file)

PID=$(powershell -Command "(Get-CimInstance Win32_Process -Filter \"Name='mergeship.exe'\" | Where-Object { \$_.ExecutablePath -eq '$EXE' }).ProcessId")
if [ -n "$PID" ]; then
    taskkill //PID $PID > /dev/null 2>&1
    for i in $(seq 1 15); do tasklist //FI "PID eq $PID" | grep -qi mergeship || break; sleep 1; done
fi
cp build/host-soh/Release/soh.dll build/latest/soh.dll && cp build/host-soh/Release/soh.pdb build/latest/soh.pdb
if [ "$(head -c2 build/latest/soh.dll)" != "MZ" ]; then
    echo "COPY FAILED"
    exit 1
fi
powershell -Command "Start-Process -FilePath '$EXE' -WorkingDirectory 'C:\Users\hyper\Mergeship2\build\latest'"
echo "IN AND OPEN: soh.dll $(date -r build/latest/soh.dll +%H:%M:%S)"
grep -E "warning C" "$LOG" | sort -u | head -5
