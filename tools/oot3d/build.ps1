# Builds tools/oot3d (oot3d-extract.exe) into build\oot3d with the host build's vcpkg (build\vcpkg, triplet
# x64-windows-static-md). Does not touch any game build.
#   powershell -File tools\oot3d\build.ps1 [-Configuration Release|Debug] [-NoConfigure]
param(
    [string]$Configuration = 'Release',
    [switch]$NoConfigure
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$vcpkgRoot = Join-Path $root 'build\vcpkg'
$triplets = Join-Path $root 'tools\triplets'
$binary = Join-Path $root 'build\oot3d'

$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake).Source }

$env:VCPKG_ROOT = $vcpkgRoot
$env:VCPKG_OVERLAY_TRIPLETS = $triplets
$env:VCPKG_DEFAULT_TRIPLET = 'x64-windows-static-md'

if (-not $NoConfigure -or -not (Test-Path (Join-Path $binary 'CMakeCache.txt'))) {
    & $cmake -S $PSScriptRoot -B $binary -G 'Visual Studio 17 2022' -A x64 `
        "-DCMAKE_TOOLCHAIN_FILE=$(Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake')" `
        '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md' "-DVCPKG_OVERLAY_TRIPLETS=$triplets" `
        '-DVCPKG_MANIFEST_MODE=OFF'
    if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }
}
& $cmake --build $binary --config $Configuration --target oot3d-extract -- /m /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
Write-Output (Join-Path $binary "$Configuration\oot3d-extract.exe")
