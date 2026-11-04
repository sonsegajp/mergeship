# MergeShip2 -- boot the game once per Majora's Mask form and report what the player was built as.
#
# This exists because "the game started and did not crash" and "the player was built as a Goron"
# look identical from outside, and for several runs they were not the same thing: an Invalid OTR
# modal kept the game alive and silent at startup. The log line is the evidence; the exit code
# alone is not.
param(
    [string]$GameDir = "C:\Users\hyper\Mergeship2\Shipwright\x64\Debug",
    [int]$Seconds = 32
)

$names = @{ 0 = "adult"; 1 = "child"; 2 = "Deku"; 3 = "Goron"; 4 = "Zora"; 5 = "Fierce Deity" }
Set-Location $GameDir
$failed = 0

foreach ($f in 2, 3, 4, 5) {
    $j = Get-Content shipofharkinian.json -Raw | ConvertFrom-Json
    $j.CVars.gGeneral.MergeShipForm = $f
    $j | ConvertTo-Json -Depth 20 | Set-Content shipofharkinian.json -Encoding utf8

    Remove-Item -Recurse -Force logs -ErrorAction SilentlyContinue
    $p = Start-Process -FilePath ".\soh.exe" -PassThru -WindowStyle Minimized
    Start-Sleep -Seconds $Seconds
    $crashed = $p.HasExited
    if (-not $crashed) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Seconds 3

    $log = "logs\Ship of Harkinian.log"
    $built = Select-String -Path $log -Pattern "player initialising as (.+)$" -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Matches[0].Groups[1].Value } | Select-Object -Last 1
    $errors = @(Select-String -Path $log -Pattern "\[error\]" -ErrorAction SilentlyContinue)

    $ok = (-not $crashed) -and ($built -eq $names[$f]) -and ($errors.Count -eq 0)
    if (-not $ok) { $failed++ }
    $status = if ($ok) { "PASS" } else { "FAIL" }
    $detail = if ($crashed) { "crashed ($($p.ExitCode))" } elseif ($errors.Count) { "$($errors.Count) error(s)" } else { "built as '$built'" }
    "{0}  form {1} ({2}): {3}" -f $status, $f, $names[$f], $detail
}

if ($failed) { "`n$failed of 4 forms failed"; exit 1 } else { "`nall 4 forms built correctly"; exit 0 }
