# Builds and stages the MergeShip2 host: libultraship.dll + mergeship.exe (host/), soh.dll (Shipwright,
# -DMERGESHIP_HOSTED=ON) and 2ship.dll (2ship2harkinian, -DMERGESHIP_HOSTED=ON), all against one shared
# vcpkg root with the x64-windows-static-md triplet (SDL2 as a DLL; tools/triplets).
#
# Build trees: build\host-lus (host/), build\host-soh, build\host-2ship. Staging: build\host.
# The standalone builds (Shipwright\build\x64, 2ship2harkinian\build) are not touched.
#
# Staged archives are the ports' own, built here from repo assets: soh.o2r and mergeship2.o2r (GenerateSohOtr, which
# chains GenerateMergeShip2Otr) and 2ship.o2r (Generate2ShipOtr). The archives made from the player's ROMs (oot.o2r,
# mm.o2r) are never staged: the game makes them on its first run (HOST_DESIGN.md, "First run: archives"), and 2Ship
# reads SoH's oot.o2r directly, so there is no oot-assets.o2r either.
#
#   powershell -File tools\build_host.ps1                 # configure + build all three, then stage
#   powershell -File tools\build_host.ps1 -Only lus       # one tree (lus | soh | 2ship), then stage
#   powershell -File tools\build_host.ps1 -StageOnly
#   powershell -File tools\build_host.ps1 -StageOnly -InstallDir D:\clean   # a fresh install: shipped files only
#   powershell -File tools\build_host.ps1 -Fast          # a code edit: see below
#
# -Fast, for iterating on code (the trees must have been configured and built once): no vcpkg install and no
# configure; a tree whose code (its sources, include\mergeship, its libultraship) has nothing newer than its DLL / exe
# is not built at all (MSBuild's own no-op pass over a tree takes minutes); a port archive is packed only when its
# asset folder has something newer than the archive (the archive targets always run otherwise, 2ship.o2r through
# Python and ZAPD); staging copies only what changed. A CMakeLists / vcpkg / generated-asset change needs a plain build.
param(
    [ValidateSet('all', 'lus', 'soh', '2ship')]
    [string]$Only = 'all',
    [string]$Configuration = 'Release',
    [switch]$StageOnly,
    [switch]$NoConfigure,
    [switch]$Fast,
    # Stage into this folder instead of build\host (an empty one gives a clean install, for the first-run test).
    [string]$InstallDir
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $root 'build'
$vcpkgRoot = Join-Path $build 'vcpkg'
$triplets = Join-Path $root 'tools\triplets'
$triplet = 'x64-windows-static-md'
$stage = if ($InstallDir) { [System.IO.Path]::GetFullPath($InstallDir) } else { Join-Path $build 'latest' }

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path $cmake)) { $cmake = $cmakeCommand.Source }

# One vcpkg root for every hosted tree, so all modules link the same package builds.
$env:VCPKG_ROOT = $vcpkgRoot
$env:VCPKG_OVERLAY_TRIPLETS = $triplets
$env:VCPKG_DEFAULT_TRIPLET = $triplet
# At most 4 files compile at once (the trees' own /MP uses every core): this machine has 16 GB, and 8 of SoH's larger
# files at once ran the compiler out of heap space (C1060) beside the user's open apps. _CL_ is appended to cl's
# command line, so its /MP4 wins; set _CL_ yourself to change it.
if (-not $env:_CL_) { $env:_CL_ = '/MP4' }
# libultraship's FetchContent patches fail under Git for Windows' default core.autocrlf=true.
$env:GIT_CONFIG_COUNT = '1'
$env:GIT_CONFIG_KEY_0 = 'core.autocrlf'
$env:GIT_CONFIG_VALUE_0 = 'false'

function Invoke-Checked([string]$what, [scriptblock]$block) {
    & $block
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit $LASTEXITCODE)" }
}

# Both games must compile against the same libultraship as the DLL the host ships.
$lusSw = (git -C (Join-Path $root 'Shipwright\libultraship') rev-parse HEAD).Trim()
$lus2s = (git -C (Join-Path $root '2ship2harkinian\libultraship') rev-parse HEAD).Trim()
if ($lusSw -ne $lus2s) {
    throw "libultraship differs: Shipwright has $lusSw, 2ship2harkinian has $lus2s. Check out the same mergeship2 commit in both."
}
Write-Output "libultraship $lusSw in both games"

if (-not (Test-Path (Join-Path $vcpkgRoot 'vcpkg.exe'))) {
    Invoke-Checked 'vcpkg bootstrap' { & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics }
}
$packages = @('zlib', 'bzip2', 'libzip', 'libpng', 'sdl2', 'sdl2-net', 'glew', 'glfw3', 'nlohmann-json',
              'tinyxml2', 'spdlog', 'libogg', 'libvorbis', 'opus', 'opusfile')
if ($Fast) { $NoConfigure = $true }
if (-not $StageOnly -and -not $Fast) {
    Invoke-Checked 'vcpkg install' {
        & (Join-Path $vcpkgRoot 'vcpkg.exe') install --triplet $triplet "--overlay-triplets=$triplets" @packages
    }
}

$toolchain = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
$common = @('-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
            "-DVCPKG_TARGET_TRIPLET=$triplet", "-DVCPKG_OVERLAY_TRIPLETS=$triplets")

# Each game tree also builds its port archive(s), from the same sources as its DLL, so their versions always match.
# -Fast: Code = the folders whose files make the tree's binary (Output); Archives = each archive target, its archive
# files and the asset folders they are packed from.
$trees = @(
    @{ Name = 'lus'; Source = 'host'; Binary = 'host-lus'; Targets = @('mergeship'); Extra = @();
       Output = "host-lus\$Configuration\mergeship.exe";
       Code = @('host', 'include\mergeship', 'Shipwright\libultraship\src', 'Shipwright\libultraship\include');
       Archives = @() },
    @{ Name = 'soh'; Source = 'Shipwright'; Binary = 'host-soh'; Targets = @('soh', 'GenerateSohOtr');
       Extra = @('-DMERGESHIP_HOSTED=ON');
       Output = "host-soh\$Configuration\soh.dll";
       Code = @('Shipwright\soh\soh', 'Shipwright\soh\src', 'Shipwright\soh\include', 'include\mergeship',
                'Shipwright\libultraship\src', 'Shipwright\libultraship\include');
       # (assets\custom\shaders is copied from libultraship by the target itself: its source is watched instead)
       Archives = @(@{ Target = 'GenerateSohOtr'; Files = @('host-soh\soh\soh.o2r', 'host-soh\soh\mergeship2.o2r');
                       Assets = @('Shipwright\soh\assets\custom', 'Shipwright\mergeship2\assets\o2r',
                                  'Shipwright\libultraship\src\fast\shaders');
                       Exclude = 'Shipwright\soh\assets\custom\shaders' }) },
    @{ Name = '2ship'; Source = '2ship2harkinian'; Binary = 'host-2ship'; Targets = @('2ship', 'Generate2ShipOtr');
       Extra = @('-DMERGESHIP_HOSTED=ON');
       Output = "host-2ship\$Configuration\2ship.dll";
       Code = @('2ship2harkinian\mm\2s2h', '2ship2harkinian\mm\src', '2ship2harkinian\mm\include',
                'include\mergeship', '2ship2harkinian\libultraship\src', '2ship2harkinian\libultraship\include');
       Archives = @(@{ Target = 'Generate2ShipOtr'; Files = @('host-2ship\mm\2ship.o2r');
                       Assets = @('2ship2harkinian\mm\assets\custom'); Exclude = '' }) }
)

# -Fast: a file under `folders` (relative to the repository) newer than `time`, or $null.
function Find-NewerThan([string[]]$folders, [datetime]$time, [string]$exclude = '') {
    $skip = if ($exclude) { Join-Path $root $exclude } else { '' }
    foreach ($folder in $folders) {
        $path = Join-Path $root $folder
        if (-not (Test-Path -LiteralPath $path)) { continue }
        $newer = Get-ChildItem -LiteralPath $path -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object { ($_.LastWriteTime -gt $time) -and (-not $skip -or -not $_.FullName.StartsWith($skip)) } |
            Select-Object -First 1
        if ($newer) { return $newer.FullName }
    }
    return $null
}

if (-not $StageOnly) {
    foreach ($tree in $trees) {
        if ($Only -ne 'all' -and $Only -ne $tree.Name) { continue }
        $src = Join-Path $root $tree.Source
        $bin = Join-Path $build $tree.Binary
        $targets = @($tree.Targets)
        if ($Fast) {
            $archiveTargets = @($tree.Archives | ForEach-Object { $_.Target })
            $targets = @($targets | Where-Object { $_ -notin $archiveTargets })
            $output = Join-Path $build $tree.Output
            $changed = if (Test-Path -LiteralPath $output) {
                Find-NewerThan $tree.Code (Get-Item -LiteralPath $output).LastWriteTime
            } else { $output }
            if ($changed) {
                Write-Output "$($tree.Name): code changed ($changed)"
            } else {
                $targets = @($targets | Where-Object { $_ -ne $tree.Targets[0] })
            }
            foreach ($archive in $tree.Archives) {
                $times = @($archive.Files | ForEach-Object {
                    $f = Join-Path $build $_
                    if (Test-Path -LiteralPath $f) { (Get-Item -LiteralPath $f).LastWriteTime } else { [datetime]::MinValue }
                })
                $oldest = $times | Sort-Object | Select-Object -First 1
                $assetChange = Find-NewerThan $archive.Assets $oldest $archive.Exclude
                if ($assetChange) {
                    Write-Output "$($tree.Name): assets changed ($assetChange)"
                    $targets += $archive.Target
                }
            }
            if ($targets.Count -eq 0) {
                Write-Output "$($tree.Name): up to date, not built"
                continue
            }
        }
        if (-not $NoConfigure) {
            Invoke-Checked "configure $($tree.Name)" { & $cmake -S $src -B $bin @common @($tree.Extra) }
        }
        Invoke-Checked "build $($tree.Name)" {
            & $cmake --build $bin --config $Configuration --target @($targets) --parallel -- /m /nologo /v:minimal
        }
    }
}

# Stage build\host: one directory, as the standalone games ship. Only this build's own outputs: no copies from the
# standalone builds and no ROM-derived archives. A missing output fails the stage rather than leaving an old file.
New-Item -ItemType Directory -Force -Path $stage | Out-Null
$lusOut = Join-Path $build "host-lus\$Configuration"
$sohOut = Join-Path $build "host-soh\$Configuration"
$shipOut = Join-Path $build "host-2ship\$Configuration"
$required = @(
    (Join-Path $lusOut 'mergeship.exe'), (Join-Path $lusOut 'libultraship.dll'),
    (Join-Path $vcpkgRoot "installed\$triplet\bin\SDL2.dll"),
    (Join-Path $sohOut 'soh.dll'), (Join-Path $shipOut '2ship.dll'),
    # The ports' own archives (fonts, shaders, custom textures; mergeship2.o2r: soh_fd's extra sounds, icons, meshes).
    (Join-Path $build 'host-soh\soh\soh.o2r'),          # GenerateSohOtr
    (Join-Path $build 'host-soh\soh\mergeship2.o2r'),   # GenerateMergeShip2Otr, chained by GenerateSohOtr
    (Join-Path $build 'host-2ship\mm\2ship.o2r')        # Generate2ShipOtr
)
$optional = @(
    (Join-Path $lusOut 'mergeship.pdb'), (Join-Path $lusOut 'libultraship.pdb'),
    (Join-Path $sohOut 'soh.pdb'), (Join-Path $shipOut '2ship.pdb')
)
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath $_) })
if ($missing.Count -gt 0) {
    throw ("Build output missing (build its tree, without -StageOnly):`n  " + ($missing -join "`n  "))
}
# (-Fast: only what differs from the staged copy.)
function Copy-Staged([string]$file) {
    $dst = Join-Path $stage (Split-Path -Leaf $file)
    if ($Fast -and (Test-Path -LiteralPath $dst)) {
        $a = Get-Item -LiteralPath $file
        $b = Get-Item -LiteralPath $dst
        if (($a.Length -eq $b.Length) -and ($a.LastWriteTime -eq $b.LastWriteTime)) { return }
    }
    Copy-Item -LiteralPath $file -Destination $stage -Force
}
foreach ($file in $required) {
    Copy-Staged $file
}
foreach ($file in $optional) {
    if (Test-Path -LiteralPath $file) {
        Copy-Staged $file
    } else {
        Write-Warning "not staged (missing): $file"
    }
}
# Superseded: 2Ship reads SoH's oot.o2r directly (2ship2harkinian MergeShipAssets.cpp); an old copy only misleads.
$obsolete = Join-Path $stage 'oot-assets.o2r'
if (Test-Path -LiteralPath $obsolete) {
    Remove-Item -LiteralPath $obsolete -Force
    Write-Output "Removed the obsolete $obsolete"
}
# The first-run extractors' files, beside the executable, as each game's post-build step lays them out: SoH's asset
# ymls (torch, for oot.o2r) and 2Ship's xmls, file lists and configs (ZAPD, for mm.o2r).
$stageAssets = Join-Path $stage 'assets'
New-Item -ItemType Directory -Force -Path $stageAssets | Out-Null
foreach ($assets in @((Join-Path $sohOut 'assets'), (Join-Path $shipOut 'assets'))) {
    if (-not (Test-Path -LiteralPath $assets)) {
        throw "Build output missing: $assets (the extractor files a first run needs)"
    }
    if ($Fast) {
        # newer files only; robocopy's exit codes below 8 are success
        & robocopy $assets $stageAssets /E /XO /NJH /NJS /NFL /NDL /NP | Out-Null
        if ($LASTEXITCODE -ge 8) { throw "staging $assets failed (robocopy exit $LASTEXITCODE)" }
        $global:LASTEXITCODE = 0
    } else {
        Get-ChildItem -LiteralPath $assets | Copy-Item -Destination $stageAssets -Recurse -Force
    }
}
foreach ($need in @('config.yml', 'xml', 'filelists', 'Config_N64_US.xml', 'Config_GC_US.xml')) {
    if (-not (Test-Path -LiteralPath (Join-Path $stageAssets $need))) {
        throw "Staged extractor files incomplete: assets\$need is missing"
    }
}
$controllerDb = Join-Path $build 'host-soh\gamecontrollerdb.txt'
if (Test-Path $controllerDb) { Copy-Item $controllerDb $stage -Force }
Write-Output "Staged $stage"
