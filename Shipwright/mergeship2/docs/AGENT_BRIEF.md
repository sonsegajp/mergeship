# MergeShip2 agent brief (shared by every parallel agent)

Read AGENTS.md and docs/PORTING_GUIDE.md first. The goal is Majora's Mask forms in Ocarina of Time (SoH),
1:1 with MM. Port MM's native code from 2ship2harkinian/mm; never approximate when MM's code exists, and never
use the original Mergeship project.

## Build and test (mandatory protocol)
- Never run cmake/soh.exe directly. Use the lock script, which serialises builds and game runs across agents:
    bash tools/locked_build_test.sh build
    bash tools/locked_build_test.sh both <testdir> [secs]     # build then run build/tests/<testdir>/script.txt
    bash tools/locked_build_test.sh test <testdir> [secs]
- Quick compile check without the lock:
    powershell -NoProfile -ExecutionPolicy Bypass -File C:\Users\hyper\Mergeship2\tools\syntax_check.ps1 <files>
- New .c/.cpp files: after creating one, run `bash tools/locked_build_test.sh configure` (never run cmake directly):
    "/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe" Shipwright/build/x64
  (only while holding nothing else; it is fast). Registration .cpp files go under soh/soh/MergeShip.
- The build is /W3 /WX. MergeShip .c files have /wd4244 /wd4305; everything else (C4013 undeclared function,
  C4047/C4024 pointer mismatch) is an error. gSPSegment needs (uintptr_t) casts. Use _strdup not strdup.
- Harness: build/tests/<yourdir>/script.txt, commands documented at the top of
  Shipwright/soh/soh/MergeShip/MergeShipTest.cpp (boot, waitplay, wait, hold, stick, press, form, givemask,
  give, cbutton, pos, health, magic, trace, mark, shot, quit). trace.log gets per-frame P (player), C (camera),
  H (hand DLs, ocarina/msg modes) lines. C code can log with `void MergeShipTest_Log(const char* fmt, ...);`
  (declare it; no-op outside tests). Screenshots are PNGs in the test dir -- look at them.
- Use a test directory named after your workstream. Evidence (trace lines, screenshots) is required before
  you call anything done. Builds and parsing are not evidence.

## Shared-file etiquette
Several agents edit z_player.c, z_player_lib.c, z_parameter.c, z_camera.c concurrently. Make small Edit-tool
hunks only; never rewrite a whole shared file, never reformat, never change line endings. Re-read a region
right before editing it. Keep your own logic in your own .inc/.c files and hook it in with one-line calls.

## Lessons already learned today (check your area for the same classes of bug)
1. OoT code gated on LINK_IS_ADULT / gSaveContext.linkAge / 0.64f runs for MM forms too. MM forms must use
   their own per-form data (this->mergeShipForm, ageProperties). Example fixed: root translation scale
   (MM uses ageProperties->unk_08: Deku 0.3, Goron 0.74, Zora 1.0; FD never scaled).
2. OoT table offsets sized for 2 ages (e.g. `PLAYER_SHIELD_MAX * 4`) are wrong for the 6-column tables; MM
   forms must use only their own column.
3. SoH enhancements hook player drawing/behaviour and assume OoT Link (customequipment.cpp replaced the Zora
   guitar hand with adult Link's hand). Gate such hooks with `player->mergeShipForm >= PLAYER_FORM_DEKU`.
4. MM continuous sounds are requested as `ID - SFX_FLAG` every frame; the raw ID restarts every frame.
5. MM display lists call segment 0x0C by index (G_DL_INDEX); use MergeShip_Scene_SetRenderModeXlu
   (soh/MergeShip/MergeShipMMScene.h), never gEmptyDL.
6. MM hand shapes can come from the animation joint table (GET_LEFT/RIGHT_HAND_INDEX_FROM_JOINT_TABLE,
   D_801C095C/D_801C0964) -- not yet ported.
7. The user tests in person; do not launch soh.exe for them. Do not commit.

## Report
Finish with: what you changed (files, hooks), the harness evidence (test dir, what the trace/screenshots show),
and anything left undone with the reason.

## Update (evening, 2026-09-22)
- Every MergeShip feature is an optional enhancement. Gate new behaviour with
  CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.<Feature>"), <default>) and add a checkbox for it to
  Shipwright/soh/soh/SohGui/SohMenuMergeShip.cpp (the "Mergeship" menu section; follow the existing widgets
  there). Keep defaults as the user specified.
- The user plays a Release build (Shipwright/x64/Release/soh.exe). While they play, the coordinator HOLDS the
  build/test lock (owner "user playing ..."); your lock-script calls will wait. That is intended: never delete
  the lock, never kill soh.exe. Use the syntax checker meanwhile:
    powershell -NoProfile -ExecutionPolicy Bypass -File "C:/Users/hyper/Mergeship2/tools/syntax_check.ps1" <files>
  (forward slashes; backslashes get eaten by bash).
- Several agents run in parallel again. Stay in your workstream's files; small Edit hunks in shared files.
- Do not stop/kill other agents' processes. Do not run cmake configure while a build is running.
