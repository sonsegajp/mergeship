# Anchor across the worlds: one connection for the whole run, whichever world the player is in.
#
#   powershell -ExecutionPolicy Bypass -File tools\test_anchor.ps1 [-Port 43383] [-QuitAfter 360]
#
# Needs a staged build (tools\build_host.ps1) and Python. Starts tools\anchor_mock_server.py on 127.0.0.1, then runs
# mergeship.exe through tools\test_host.ps1 -Mode script with the two scripts below (the door test's route, with
# Anchor on):
#   SoH    boots the Lost Woods, enables Anchor (room "mergeship-test"), shows its loopback player, asks the mock
#          server's bot to "find" the Boomerang in Hyrule 25 s later (MOCK_ECHO), and takes the wrong turn to Termina;
#   2Ship  arrives in South Clock Town (a new Majora's Mask file, intro and first cycle skipped), shows its loopback
#          player (Majora's Mask's Player as a dummy), waits while the bot's item arrives, walks back through the Clock
#          Tower to Hyrule;
#   SoH    logs its state and goes to Termina again.
# Every suspend and resume logs "ANCHOR <suspended|resumed>: engine ..., connection N, clientId X" (both games' traces
# and the host log); the host logs "net: active engine N, Anchor connection N up" at each switch.
#
# Then tools\anchor_check.py decides (anchor_result.txt): PASS if the host connected once (connection 1), never closed
# it during the run, every switch found connection 1 up, both engines resumed on it with one client id, the mock
# server saw one TCP connection and one handshake, and the bot's item was applied exactly once (received in Termina
# through the shared save, or, if the item-delivery projector does not take it, given in Hyrule on the way back).
#
# Output: build\tests\anchor_<timestamp>\ (scripts, traces, host log, game logs, screenshots, server log, result).
param(
    [int]$Port = 43383,
    [double]$QuitAfter = 420,
    [int]$Transfers = 3,
    [double]$Settle = 15
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$out = Join-Path $root "build\tests\anchor_$stamp"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$soh = @"
boot 0x11E child
waitplay 30
mark anchor-enable
anchor enable 127.0.0.1 $Port mergeship-test Link default
wait 120
anchor state
anchor loop 255 80 80 80 0
wait 20
shot soh_anchor_loopback
# The mock server's bot finds the Boomerang in Hyrule 25 s from now, when the player is in Termina.
anchor send {"type":"MOCK_ECHO","delayMs":25000,"packet":{"type":"GIVE_ITEM","targetTeamId":"default","addToQueue":true,"modId":0,"getItemId":6,"msItem":"BOOMERANG"}}
walkexit 1 400 1
# (in Termina until the player comes back through the Clock Tower)
waitplay 20
mark back-in-hyrule
anchor state
shot soh_anchor_back
wait 60
walkexit 1 400 1
"@
$mm = @'
cvar gEnhancements.Cutscenes.SkipIntroSequence 1
cvar gEnhancements.Cutscenes.SkipFirstCycle 1
waitplay 40
mark arrived-termina
anchor state
anchor loop 80 80 255 80 0
wait 20
anchor dummies
shot mm_anchor_loopback
# The bot's Hyrule item arrives during this wait.
wait 600
anchor state
walkexit 1 300
waitplay 30
pos 0 -280 -820 -32768
wait 30
walkexit 1 400
# (back in Hyrule; the next trip resumes here)
waitplay 40
mark arrived-again
anchor state
shot mm_anchor_again
'@
$sohPath = Join-Path $out 'script_soh.txt'
$mmPath = Join-Path $out 'script_2ship.txt'
Set-Content -Path $sohPath -Value $soh -Encoding ascii
Set-Content -Path $mmPath -Value $mm -Encoding ascii

# The mock Anchor server
$serverLog = Join-Path $out 'anchor_server.log'
$server = Start-Process -FilePath 'python' -PassThru -WindowStyle Hidden `
    -ArgumentList @((Join-Path $root 'tools\anchor_mock_server.py'), '--port', $Port, '--log', $serverLog) `
    -RedirectStandardOutput (Join-Path $out 'anchor_server_stdout.txt') `
    -RedirectStandardError (Join-Path $out 'anchor_server_stderr.txt')
Start-Sleep -Seconds 2
if ($server.HasExited) { throw "the mock Anchor server did not start (see $out\anchor_server_stderr.txt)" }

$started = Get-Date
try {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $root 'tools\test_host.ps1') -Mode script `
        -SohScript $sohPath -MMScript $mmPath -Transfers $Transfers -Settle $Settle -QuitAfter $QuitAfter
    $hostCode = $LASTEXITCODE
} finally {
    Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
}

# test_host.ps1's own output directory (screenshots, host log, game logs): into ours.
$hostRun = Get-ChildItem (Join-Path $root 'build\tests') -Directory -Filter 'host_script_*' |
    Where-Object { $_.CreationTime -ge $started.AddSeconds(-5) } | Sort-Object CreationTime | Select-Object -Last 1
if ($hostRun) {
    Get-ChildItem $hostRun.FullName -File | Where-Object { $_.Name -notlike 'script_*' } |
        Copy-Item -Destination $out -Force
    "host run: $($hostRun.FullName)" | Add-Content (Join-Path $out 'anchor_result_source.txt')
}
"host exit code $hostCode" | Add-Content (Join-Path $out 'anchor_result_source.txt')

& python (Join-Path $root 'tools\anchor_check.py') $out
$checkCode = $LASTEXITCODE
Write-Output "mergeship.exe exit code $hostCode; results in $out"
if ($hostCode -ne 0) { exit $hostCode }
exit $checkCode
