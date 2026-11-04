# MergeShip2 -- compile Majora's Mask's player against Ocarina of Time's engine and report
# what still does not resolve. This is the worklist for the compatibility layer: each run either
# names the next missing header or, once the includes resolve, the remaining symbol mismatches.
param([string]$File = "C:\Users\hyper\Mergeship2\Shipwright\soh\src\overlays\actors\ovl_player_actor\z_player_mm.c")

$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$inc = @("C:\Users\hyper\Mergeship2\Shipwright\soh\soh\MergeShip\mmcompat") +
       (Get-Content "C:\Users\hyper\Mergeship2\Shipwright\build\x64\soh\mm_incdirs.txt") +
       @("C:\Users\hyper\Mergeship2\Shipwright\soh\soh\MergeShip\mmcompat\fallback")
$incArgs = $inc | ForEach-Object { "/I`"$_`"" }
$defs = (Get-Content "C:\Users\hyper\Mergeship2\Shipwright\build\x64\soh\mm_defines.txt") | ForEach-Object { "/D$_" }
$a = @('/c','/nologo','/TC','/W0','/Zs','/std:c11','/Zc:preprocessor',
       '/FI"soh/MergeShip/MergeShipMMCompat.h"') + $incArgs + $defs + @("`"$File`"")
$cmd = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>&1 && cl " + ($a -join ' ')
$out = & "$env:SystemRoot\System32\cmd.exe" /c $cmd 2>&1
$out | Set-Content "C:\Users\hyper\Mergeship2\mm_player_errors.txt"

$fatal = $out | Select-String -Pattern "fatal error C1083.*'([^']+)'"
if ($fatal) { "NEXT MISSING HEADER: " + $fatal[0].Matches[0].Groups[1].Value; exit 1 }

$errs = $out | Select-String -Pattern ": error "
"errors: $($errs.Count)"
$out | Select-String -Pattern "error (C\d+)" | ForEach-Object { $_.Matches[0].Groups[1].Value } |
    Group-Object | Sort-Object Count -Descending | Select-Object -First 8 Count,Name | Format-Table -AutoSize
if ($errs.Count -eq 0) { "MM's player compiles against OoT's engine"; exit 0 } else { exit 1 }

