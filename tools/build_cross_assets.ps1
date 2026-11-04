# Build, export, verify, and stage the OoT-assets-in-MM test package.
param(
    [string]$Rom,
    [string]$Configuration = "Debug",
    [switch]$SkipBuild,
    [switch]$SkipExport,
    [switch]$Launch
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exportDir = Join-Path $repoRoot 'build\oot-assets'
$stageDir = Join-Path $repoRoot 'build\runtime-mm'
$torch = Join-Path $repoRoot "Shipwright\build\x64\$Configuration\soh-torch.exe"
$mmDir = Join-Path $repoRoot "2ship2harkinian\x64\$Configuration"
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } else {
    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if (-not $SkipExport -and -not $Rom) {
    throw 'Pass -Rom with your own supported OoT ROM, or -SkipExport to verify an existing export.'
}
if ($Rom) { $Rom = (Resolve-Path -LiteralPath $Rom).Path }
Push-Location $repoRoot
try {
    if (-not $SkipBuild) {
        & $cmake --build Shipwright\build\x64 --config $Configuration --target soh-torch --parallel 6
        if ($LASTEXITCODE -ne 0) { throw 'OoT exporter build failed' }
        & $cmake --build 2ship2harkinian\build\x64 --config $Configuration --target 2ship --parallel 6
        if ($LASTEXITCODE -ne 0) { throw 'MM runtime build failed' }
    }
    New-Item -ItemType Directory -Path $exportDir -Force | Out-Null
    if (-not $SkipExport) {
        & $torch --src Shipwright\soh\assets\yml --dest $exportDir --version 9.2.3 --namespace oot $Rom
        if ($LASTEXITCODE -ne 0) { throw 'Namespaced OoT extraction failed' }
    }
    # Compare to the user's stock archive; no ROM-derived assets are distributed with source.
    & python tools\verify_asset_namespace.py Shipwright\x64\Debug\oot.o2r `
        "$exportDir\oot-assets.o2r" --report "$exportDir\namespace-report.json"
    if ($LASTEXITCODE -ne 0) { throw 'OoT namespace verification failed' }
    $required = @("$mmDir\2ship.exe", "$mmDir\assets", '2ship2harkinian\2ship.o2r',
                  '2ship2harkinian\mm.o2r', "$exportDir\oot-assets.o2r")
    foreach ($path in $required) {
        if (-not (Test-Path -LiteralPath $path)) { throw "Required package input missing: $path" }
    }
    New-Item -ItemType Directory -Path $stageDir -Force | Out-Null
    Copy-Item -LiteralPath "$mmDir\2ship.exe" -Destination $stageDir -Force
    Copy-Item -LiteralPath '2ship2harkinian\2ship.o2r' -Destination $stageDir -Force
    Copy-Item -LiteralPath '2ship2harkinian\mm.o2r' -Destination $stageDir -Force
    Copy-Item -LiteralPath "$exportDir\oot-assets.o2r" -Destination $stageDir -Force
    # Copy contents, not the directory itself, to avoid assets/assets on subsequent runs.
    $stageAssets = Join-Path $stageDir 'assets'
    New-Item -ItemType Directory -Path $stageAssets -Force | Out-Null
    Get-ChildItem -LiteralPath "$mmDir\assets" | Copy-Item -Destination $stageAssets -Recurse -Force
    Write-Output "MM test package: $stageDir\2ship.exe"
    Write-Output 'This package verifies cross-game assets; OoT transformation gameplay remains unfinished.'
    if ($Launch) {
        Start-Process -FilePath "$stageDir\2ship.exe" -WorkingDirectory $stageDir
    }
} finally { Pop-Location }
