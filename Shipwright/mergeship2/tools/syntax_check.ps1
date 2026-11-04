# MergeShip2 -- syntax-check SoH C/C++ sources with the soh target's include paths and defines,
# without waiting for a full build. Usage: syntax_check.ps1 file1 [file2 ...]
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Files)
$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$root = "C:\Users\hyper\Mergeship2\Shipwright"
$inc = @("C:\Users\hyper\Mergeship2\include") + (Get-Content "$root\build\x64\soh\mm_incdirs.txt")
$defs = Get-Content "$root\build\x64\soh\mm_defines.txt"
$fail = 0
foreach ($f in $Files) {
    $rsp = [System.IO.Path]::GetTempFileName()
    $lines = @('/c', '/nologo', '/W1', '/Zs', '/Zc:preprocessor', '/utf-8', '/bigobj', '/permissive-')
    if ($f -like "*.cpp") { $lines += @('/TP', '/std:c++20', '/EHsc') } else { $lines += @('/TC', '/std:c11') }
    $lines += $inc | ForEach-Object { "/I`"$_`"" }
    # In a response file a define's quotes survive when escaped with a backslash.
    $lines += $defs | ForEach-Object { "/D" + ($_ -replace '"', '\"') }
    $lines += "`"$f`""
    Set-Content -Path $rsp -Value $lines -Encoding ascii
    $cmd = "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>&1 && cl @`"$rsp`""
    $out = & "$env:SystemRoot\System32\cmd.exe" /c $cmd 2>&1
    Remove-Item $rsp
    $errs = @($out | Select-String -Pattern ": (fatal )?error ")
    "{0}: {1} error(s)" -f (Split-Path $f -Leaf), $errs.Count
    $errs | Select-Object -First 30 | ForEach-Object { "   " + $_.Line }
    if ($errs.Count) { $fail++ }
}
exit $fail
