# Runs mergeship.exe (staged by tools\build_host.ps1) muted, and keeps its logs and screenshots.
#
#   powershell -File tools\test_host.ps1 -Mode switch -Switches 50   # SoH title, 2Ship title, SoH again, N switches
#   powershell -File tools\test_host.ps1 -Mode only-soh              # one engine, like the standalone exe
#   powershell -File tools\test_host.ps1 -Mode only-2ship
#   powershell -File tools\test_host.ps1 -Mode transfer          # the door (M3) and the shared save (M4), below
#   powershell -File tools\test_host.ps1 -Mode transfer-opening  # first trip: Majora's Mask's own opening
#   powershell -File tools\test_host.ps1 -Mode script -SohScript a.txt [-MMScript b.txt] [-Only soh] [-Transfers N]
#
# Transfer modes drive both games with their test scripts (SoH: MERGESHIP_TEST_SCRIPT_SOH, MergeShipTest.cpp; 2Ship:
# MERGESHIP_TEST_SCRIPT_2SHIP, MergeShipHostTest.cpp), written into the output directory unless given, and run the
# host with --transfer-test N: it screenshots every arrival and passes after the N-th. The SoH script runs the
# unsaved debug file, whose Majora's Mask file is the third (build\host\saves\file3.json, deleted first).
#   transfer          SoH boots in the Lost Woods, owns a Majora's Mask mask (Great Fairy's), takes a wrong turn ->
#                     Termina (a new MM file; MM's intro and first cycle skipped with 2Ship's own options, so the shared
#                     save takes part) -> South Clock Town; MM gets the Bunny Hood, walks into the Clock Tower, through
#                     the stone doors and the passage -> Hyrule's Lost Woods (the Bunny Hood is now Hyrule's); SoH
#                     takes another wrong turn -> South Clock Town again, the same cycle; the Great Fairy's Mask is on
#                     MM's mask page.
#   transfer-opening  the first trip with nothing skipped: Majora's Mask's opening plays.
#
# Output: build\tests\host_<mode>_<timestamp>\ (PNG screenshots, host log, both games' logs, exit code).
param(
    [ValidateSet('switch', 'hotkey', 'only-soh', 'only-2ship', 'transfer', 'transfer-opening', 'script')]
    [string]$Mode = 'switch',
    [int]$Switches = 50,
    [double]$SwitchSeconds = 0.75,
    [double]$QuitAfter = 25,
    [int]$TimeoutSeconds = 600,
    [string]$SohScript = '',
    [string]$MMScript = '',
    [string]$Only = '',
    [int]$Transfers = 0,
    [double]$Settle = 12,
    # Run in build\host itself (the user's play directory) instead of the private copy build\hostrun.
    [switch]$InPlace
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$stage = if ($env:MS_STAGE) { $env:MS_STAGE } else { Join-Path $root 'build\host' } # MS_STAGE: another staged build
if (-not (Test-Path (Join-Path $stage 'mergeship.exe'))) { throw "Not staged: $stage\mergeship.exe (run tools\build_host.ps1)" }
if (-not $InPlace) {
    # The user plays from build\host: run from a private copy (build\hostrun) so a test never touches their saves,
    # settings or log. Binaries and archives are refreshed when newer; saves start empty.
    $private = Join-Path $root 'build\hostrun'
    New-Item -ItemType Directory -Force -Path $private | Out-Null
    Get-ChildItem -LiteralPath $stage -File | Where-Object { $_.Extension -in '.exe', '.dll', '.pdb', '.o2r', '.txt', '.json', '.ini' } |
        ForEach-Object {
            $dst = Join-Path $private $_.Name
            if (-not (Test-Path $dst) -or ($_.LastWriteTime -gt (Get-Item $dst).LastWriteTime) -or ($_.Extension -in '.json', '.ini')) {
                Copy-Item -LiteralPath $_.FullName -Destination $dst -Force
            }
        }
    foreach ($d in 'assets', 'randomizer', 'presets') {
        $src = Join-Path $stage $d
        if ((Test-Path $src) -and -not (Test-Path (Join-Path $private $d))) { Copy-Item -Recurse $src $private }
    }
    foreach ($d in 'Save', 'saves', 'logs') { Remove-Item -Recurse -Force -ErrorAction SilentlyContinue (Join-Path $private $d) }
    $stage = $private
}
$exe = Join-Path $stage 'mergeship.exe'

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$out = Join-Path $root "build\tests\host_$($Mode -replace '-', '_')_$stamp"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$arguments = @('--muted', '--shots', $out, '--record', (Join-Path $out 'frames.mp4'))
switch ($Mode) {
    'switch' { $arguments += @('--switch-test', $Switches, '--switch-seconds', $SwitchSeconds) }
    'hotkey' { $arguments += @('--quit-after', 60, '--debug-switch') }
    'only-soh' { $arguments += @('--only', 'soh', '--quit-after', $QuitAfter) }
    'only-2ship' { $arguments += @('--only', '2ship', '--quit-after', $QuitAfter) }
}

# ---- the door's test scripts --------------------------------------------------------------------------------------
# SoH: ENTR_LOST_WOODS_SOUTH_EXIT (0x11E) is the woods' first clearing, entered from Kokiri Forest. Its exit-1 openings
# (the loop back to Kokiri Forest) are the way back behind Link (the nearest, k=0) and the wrong turn straight ahead
# (k=1): "walkexit 1 400 1" walks into the wrong turn.
$sohTransfer = @'
boot 0x11E child
waitplay 30
mark lost-woods
exits 1
givemmmask 11
masks
shot soh_01_lost_woods
walkexit 1 400 1
# (in Termina until the player comes back through the Clock Tower)
waitplay 20
mark back-in-hyrule
masks
shot soh_02_back_in_lost_woods
wait 40
walkexit 1 400 1
'@
$mmTransfer = @'
# Skip Majora's Mask's opening and first cycle with 2Ship's own options, so the shared save takes part at once.
cvar gEnhancements.Cutscenes.SkipIntroSequence 1
cvar gEnhancements.Cutscenes.SkipFirstCycle 1
waitplay 40
mark arrived-south-clock-town
inventory
shot mm_01_south_clock_town
give 0x39
inventory
walkexit 1 300
waitplay 30
mark clock-tower-interior
exits
actors 0x061
shot mm_02_clock_tower
# Straight-line steering cannot get round the Salesman room's pillars: start in the passage.
pos 0 -280 -820 -32768
wait 30
shot mm_02b_passage
walkexit 1 400
# (back in Hyrule; the next trip resumes here)
waitplay 40
mark arrived-again
inventory
shot mm_03_south_clock_town_again
press S
wait 40
press Z
wait 40
shot mm_04_mask_page
press S
wait 20
'@
$sohOpening = @'
boot 0x11E child
waitplay 30
shot soh_01_lost_woods
walkexit 1 400 1
'@
$mmOpening = @'
wait 150
shot mm_opening_05s
wait 300
shot mm_opening_15s
wait 450
shot mm_opening_30s
wait 900
shot mm_opening_60s
'@

function Set-Script([string]$name, [string]$given, [string]$default) {
    if ($given) { return (Resolve-Path $given).Path }
    $path = Join-Path $out $name
    Set-Content -Path $path -Value $default -Encoding ascii
    return $path
}
Remove-Item Env:MERGESHIP_TEST_SCRIPT -ErrorAction SilentlyContinue
Remove-Item Env:MERGESHIP_TEST_SCRIPT_SOH -ErrorAction SilentlyContinue
Remove-Item Env:MERGESHIP_TEST_SCRIPT_2SHIP -ErrorAction SilentlyContinue
if ($Mode -in @('transfer', 'transfer-opening', 'script')) {
    if ($Mode -eq 'transfer') {
        $env:MERGESHIP_TEST_SCRIPT_SOH = Set-Script 'script_soh.txt' $SohScript $sohTransfer
        $env:MERGESHIP_TEST_SCRIPT_2SHIP = Set-Script 'script_2ship.txt' $MMScript $mmTransfer
        if ($Transfers -eq 0) { $Transfers = 3 }
        if ($QuitAfter -eq 25) { $QuitAfter = 300 }
    } elseif ($Mode -eq 'transfer-opening') {
        $env:MERGESHIP_TEST_SCRIPT_SOH = Set-Script 'script_soh.txt' $SohScript $sohOpening
        $env:MERGESHIP_TEST_SCRIPT_2SHIP = Set-Script 'script_2ship.txt' $MMScript $mmOpening
        if ($Transfers -eq 0) { $Transfers = 1 }
        if ($Settle -eq 12) { $Settle = 70 }
        if ($QuitAfter -eq 25) { $QuitAfter = 200 }
    } else {
        if ($SohScript) { $env:MERGESHIP_TEST_SCRIPT_SOH = (Resolve-Path $SohScript).Path }
        if ($MMScript) { $env:MERGESHIP_TEST_SCRIPT_2SHIP = (Resolve-Path $MMScript).Path }
    }
    if ($Only) { $arguments += @('--only', $Only) }
    if ($Transfers -gt 0) {
        $arguments += @('--transfer-test', $Transfers, '--settle', $Settle)
    }
    $arguments += @('--quit-after', $QuitAfter)
    # The debug file's Majora's Mask file (the third) starts fresh; so does its shared save (kept in memory only).
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $stage 'saves\file3*.json')
}

# With the displays asleep (unattended runs), DWM paces V-Sync'd presents at ~7.6 Hz and the games crawl
# through their title sequences. The test runs turn V-Sync off in the staged configs (build\host only).
$vsyncOff = @'
import json, os, sys
for name in ("shipofharkinian.json", "2ship2harkinian.json"):
    path = os.path.join(sys.argv[1], name)
    data = json.load(open(path)) if os.path.exists(path) else {}
    data.setdefault("CVars", {}).setdefault("gSettings", {})["VsyncEnabled"] = 0
    # Anchor: never the player's own room (the staged settings are theirs: room, name, anchor.hm64.org). Off unless
    # MS_ANCHOR=1, and then the local mock server (tools/anchor_mock_server.py) under a test name and room.
    anchor = data["CVars"].setdefault("gRemote", {}).setdefault("Anchor", {})
    if os.environ.get("MS_ANCHOR") == "1":
        anchor.update({"Enabled": 1, "Host": "127.0.0.1", "Port": int(os.environ.get("MS_ANCHOR_PORT", "43383")),
                       "RoomId": "mergeship-test", "Name": "TestLink", "TeamId": "default"})
    else:
        anchor["Enabled"] = 0
    # Off screen, as run_final.sh (the user, 2026-10-07: tests run muted and unseen); MS_VISIBLE=1 puts it on screen.
    if os.environ.get("MS_VISIBLE") != "1":
        data.setdefault("Window", {}).update({"PositionX": -4000, "PositionY": 40})
    json.dump(data, open(path, "w"), indent=4)
'@
$vsyncScript = Join-Path $out 'vsync_off.py'
Set-Content -Path $vsyncScript -Value $vsyncOff -Encoding ascii
& python $vsyncScript $stage
if ($LASTEXITCODE -ne 0) { throw 'could not turn V-Sync off in the staged configs' }

# The games write their logs into logs\ next to the exe; start from empty ones so they belong to this run.
Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $stage 'logs\*.log')

$process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $stage -PassThru `
    -RedirectStandardOutput (Join-Path $out 'stdout.txt') -RedirectStandardError (Join-Path $out 'stderr.txt')
$null = $process.Handle # keeps the exit code readable after the process ends
if ($Mode -eq 'hotkey') {
    # Press Ctrl+F12 twice (SoH -> 2Ship -> SoH) the way a player would: focus the window, hold the keys.
    Add-Type -Namespace MsTest -Name Keys -MemberDefinition @'
[DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, System.UIntPtr extra);
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(System.IntPtr hwnd);
'@
    $shell = New-Object -ComObject WScript.Shell
    foreach ($at in @(20, 40)) {
        while (((Get-Date) - $process.StartTime).TotalSeconds -lt $at) { Start-Sleep -Milliseconds 200 }
        $process.Refresh()
        [void]$shell.AppActivate($process.Id)
        [void][MsTest.Keys]::SetForegroundWindow($process.MainWindowHandle)
        Start-Sleep -Milliseconds 300
        [MsTest.Keys]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)   # Ctrl down
        [MsTest.Keys]::keybd_event(0x7B, 0, 0, [UIntPtr]::Zero)   # F12 down
        Start-Sleep -Milliseconds 250
        [MsTest.Keys]::keybd_event(0x7B, 0, 2, [UIntPtr]::Zero)   # F12 up
        [MsTest.Keys]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)   # Ctrl up
        "pressed Ctrl+F12 at $at s" | Add-Content (Join-Path $out 'hotkey.txt')
    }
}
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force
    "TIMEOUT after $TimeoutSeconds s" | Set-Content (Join-Path $out 'exitcode.txt')
    throw "mergeship.exe did not finish in $TimeoutSeconds s; killed. See $out"
}
$code = $process.ExitCode
"$code" | Set-Content (Join-Path $out 'exitcode.txt')

Copy-Item -Force (Join-Path $stage 'mergeship-host.log') $out -ErrorAction SilentlyContinue
Remove-Item Env:MERGESHIP_TEST_SCRIPT_SOH -ErrorAction SilentlyContinue
Remove-Item Env:MERGESHIP_TEST_SCRIPT_2SHIP -ErrorAction SilentlyContinue
if (Test-Path (Join-Path $stage 'logs')) {
    Get-ChildItem (Join-Path $stage 'logs') -Filter '*.log' | Copy-Item -Destination $out -Force
}
# BMP -> PNG
Get-ChildItem $out -Filter '*.bmp' | ForEach-Object {
    $png = [IO.Path]::ChangeExtension($_.FullName, '.png')
    & python -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2])" $_.FullName $png
    if ($LASTEXITCODE -eq 0) { Remove-Item $_.FullName }
}
# Contact sheets of the recording: every frame, stamped with its frame number, 100 to a sheet (tools/make_sheets.sh).
$video = Join-Path $out 'frames.mp4'
for ($i = 0; ($i -lt 60) -and (Get-Process ffmpeg -ErrorAction SilentlyContinue); $i++) { Start-Sleep 1 }
if (Test-Path $video) { & bash (Join-Path $root 'tools/make_sheets.sh') ($out.Replace('\', '/')) }
Write-Output "exit code $code; results in $out"
Get-ChildItem $out | Select-Object Name, Length | Format-Table | Out-String | Write-Output
exit $code
