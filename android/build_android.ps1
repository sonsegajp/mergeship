# MergeShip2 for Android (arm64): builds the native libraries and the APK.
#
#   powershell -File android\build_android.ps1             # build what changed, package the APK
#   powershell -File android\build_android.ps1 -Install    # ... and install it on the phone (adb)
#   powershell -File android\build_android.ps1 -Configure  # re-run CMake for the three trees first
#
# Trees (build\android): deps-arm64 (android\deps: SDL2, SDL2_net, Ogg, Vorbis, Opus, OpusFile, libzip, libpng, json,
# tinyxml2, spdlog, installed into prefix-arm64), lus-arm64 (host\: libultraship.so + libmain.so), soh-arm64
# (Shipwright, libsoh.so) and 2ship-arm64 (2ship2harkinian, lib2ship.so), all -DMERGESHIP_HOSTED=ON and USE_OPENGLES.
# The APK (android\app) packages them with libc++_shared.so and SDL's Java activity; it is signed with the SDK's
# debug key (a personal build).
#
# The phone needs the archives in the app's folder, Android/data/com.mergeship2.app/files: soh.o2r, mergeship2.o2r and
# 2ship.o2r (from build\host7), and oot.o2r and mm.o2r (made from your ROMs by the Windows build). -PushArchives copies
# them there with adb.
param(
    [switch]$Configure,
    [switch]$Install,
    [switch]$PushArchives,
    [string]$ArchiveDir = '',
    [int]$Jobs = 6
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$out = Join-Path $root 'build\android'
$abi = 'arm64-v8a'
$sdk = 'C:\Android\sdk'
$ndk = Join-Path $sdk 'ndk\28.2.13676358'
$ninja = Join-Path $sdk 'cmake\3.22.1\bin\ninja.exe'
$adb = Join-Path $sdk 'platform-tools\adb.exe'
$gradle = 'C:\Android\gradle-8.7\bin\gradle.bat'
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$prefix = Join-Path $out 'prefix-arm64'
$toolchain = Join-Path $ndk 'build\cmake\android.toolchain.cmake'
if (-not $env:JAVA_HOME) { $env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-17.0.18.8-hotspot' }
# libultraship's FetchContent patches fail under Git for Windows' default core.autocrlf=true.
$env:GIT_CONFIG_COUNT = '1'; $env:GIT_CONFIG_KEY_0 = 'core.autocrlf'; $env:GIT_CONFIG_VALUE_0 = 'false'

function Invoke-Checked([string]$what, [scriptblock]$block) {
    & $block
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit $LASTEXITCODE)" }
}

$common = @('-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$ninja", "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
            "-DANDROID_ABI=$abi", '-DANDROID_PLATFORM=android-26', '-DANDROID_STL=c++_shared',
            '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_PREFIX_PATH=$prefix", "-DCMAKE_FIND_ROOT_PATH=$prefix")

# 1. The dependencies (once).
if (-not (Test-Path (Join-Path $prefix 'lib\libSDL2.so'))) {
    $deps = Join-Path $out 'deps-arm64'
    Invoke-Checked 'configure deps' { & $cmake -S (Join-Path $root 'android\deps') -B $deps @common "-DMS_PREFIX=$prefix" }
    Invoke-Checked 'build deps' { & $cmake --build $deps -j 4 }
}

# 2. The three native trees.
$gameOptions = @('-DMERGESHIP_HOSTED=ON', '-DUSE_OPENGLES=ON', "-DOPUSFILE_INCLUDE_DIR=$prefix/include/opus")
$trees = @(
    @{ Name = 'lus'; Source = 'host'; Binary = 'lus-arm64'; Target = 'main'; Extra = @() },
    @{ Name = 'soh'; Source = 'Shipwright'; Binary = 'soh-arm64'; Target = 'soh'; Extra = $gameOptions },
    @{ Name = '2ship'; Source = '2ship2harkinian'; Binary = '2ship-arm64'; Target = '2ship'; Extra = $gameOptions }
)
foreach ($tree in $trees) {
    $bin = Join-Path $out $tree.Binary
    if ($Configure -or -not (Test-Path (Join-Path $bin 'build.ninja'))) {
        Invoke-Checked "configure $($tree.Name)" { & $cmake -S (Join-Path $root $tree.Source) -B $bin @common @($tree.Extra) }
    }
    Invoke-Checked "build $($tree.Name)" { & $cmake --build $bin --target $tree.Target -j $Jobs }
}

# 3. The APK's native libraries and SDL's Java sources.
$jni = Join-Path $out "jniLibs\$abi"
New-Item -ItemType Directory -Force $jni | Out-Null
$libs = @(
    (Join-Path $prefix 'lib\libSDL2.so'),
    (Join-Path $prefix 'lib\libSDL2_net.so'),
    (Join-Path $ndk 'toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\lib\aarch64-linux-android\libc++_shared.so'),
    (Join-Path $out 'lus-arm64\libultraship\src\libultraship.so'),
    (Join-Path $out 'lus-arm64\libmain.so'),
    (Get-ChildItem (Join-Path $out 'soh-arm64') -Recurse -Filter 'libsoh.so' | Select-Object -First 1).FullName,
    (Get-ChildItem (Join-Path $out '2ship-arm64') -Recurse -Filter 'lib2ship.so' | Select-Object -First 1).FullName
)
foreach ($lib in $libs) {
    if (-not $lib -or -not (Test-Path $lib)) { throw "missing native library: $lib" }
    Copy-Item $lib $jni -Force
}
$sdlJava = Join-Path $out 'sdl-java\org\libsdl\app'
New-Item -ItemType Directory -Force $sdlJava | Out-Null
Copy-Item (Join-Path $out 'deps-arm64\sdl2-prefix\src\sdl2\android-project\app\src\main\java\org\libsdl\app\*.java') $sdlJava -Force

# 4. The APK.
$localProperties = Join-Path $root 'android\local.properties'
if (-not (Test-Path $localProperties)) { Set-Content $localProperties "sdk.dir=$($sdk -replace '\\','/')" -Encoding ascii }
Invoke-Checked 'gradle' { & $gradle -p (Join-Path $root 'android') --no-daemon assembleRelease }
$apk = Join-Path $root 'android\app\build\outputs\apk\release\app-release.apk'
Write-Host "APK: $apk"

if ($Install) {
    Invoke-Checked 'adb install' { & $adb install -r $apk }
}
if ($PushArchives) {
    $from = if ($ArchiveDir) { $ArchiveDir } else { Join-Path $root 'build\host7' }
    $dest = '/sdcard/Android/data/com.mergeship2.app/files/'
    & $adb shell mkdir -p $dest | Out-Null
    foreach ($name in @('soh.o2r', 'mergeship2.o2r', '2ship.o2r', 'oot.o2r', 'mm.o2r')) {
        $file = Join-Path $from $name
        if (Test-Path $file) { Invoke-Checked "adb push $name" { & $adb push $file $dest } } else { Write-Host "not found: $file" }
    }
}
