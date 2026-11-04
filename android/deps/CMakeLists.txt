# MergeShip2 for Android: the libraries the three native trees (android/build_android.ps1) need and vcpkg provides on
# Windows, built once for one ABI and installed into one prefix, so every tree links the same builds. SDL2 is the one
# shared library (libSDL2.so, which SDL's Java activity loads); the rest are static and linked into each module that
# uses them, as vcpkg's static triplet does on Windows.
#
#   cmake -S android/deps -B build/android/deps-<abi> -G Ninja -DCMAKE_TOOLCHAIN_FILE=<ndk>/build/cmake/android.toolchain.cmake
#         -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_shared -DMS_PREFIX=<prefix>
cmake_minimum_required(VERSION 3.24)
project(mergeship_android_deps NONE)

include(ExternalProject)

if(NOT MS_PREFIX)
    message(FATAL_ERROR "MS_PREFIX (the install prefix) is required")
endif()

# Every dependency is configured with the same Android toolchain and finds the ones before it in the prefix.
set(MS_COMMON_ARGS
    -G Ninja
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    -DANDROID_ABI=${ANDROID_ABI}
    -DANDROID_PLATFORM=${ANDROID_PLATFORM}
    -DANDROID_STL=${ANDROID_STL}
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=${MS_PREFIX}
    -DCMAKE_PREFIX_PATH=${MS_PREFIX}
    -DCMAKE_FIND_ROOT_PATH=${MS_PREFIX}
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
)

function(ms_dep name repo tag)
    ExternalProject_Add(${name}
        GIT_REPOSITORY ${repo}
        GIT_TAG ${tag}
        GIT_SHALLOW TRUE
        GIT_CONFIG core.autocrlf=false
        UPDATE_DISCONNECTED TRUE
        CMAKE_ARGS ${MS_COMMON_ARGS} ${ARGN}
    )
endfunction()

ms_dep(sdl2 https://github.com/libsdl-org/SDL.git release-2.32.10
    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_TESTS=OFF)

ms_dep(sdl2_net https://github.com/libsdl-org/SDL_net.git release-2.2.0
    -DBUILD_SHARED_LIBS=ON -DSDL2NET_SAMPLES=OFF -DSDL2NET_INSTALL=ON)
add_dependencies(sdl2_net sdl2)

ms_dep(ogg https://github.com/xiph/ogg.git v1.3.5
    -DBUILD_SHARED_LIBS=OFF -DINSTALL_DOCS=OFF -DBUILD_TESTING=OFF)

ms_dep(vorbis https://github.com/xiph/vorbis.git v1.3.7
    -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF)
add_dependencies(vorbis ogg)

ms_dep(opus https://github.com/xiph/opus.git v1.5.2
    -DOPUS_BUILD_SHARED_LIBRARY=OFF -DOPUS_BUILD_TESTING=OFF -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_INSTALL_PKG_CONFIG_MODULE=ON
    -DOPUS_INSTALL_CMAKE_CONFIG_MODULE=ON)

# (opusfile's last release, 0.12, predates its CMake build: a later commit of master.)
ExternalProject_Add(opusfile
    GIT_REPOSITORY https://github.com/xiph/opusfile.git
    GIT_TAG 3ecc22aa0a4430f61c2403d57370a089536d197a
    GIT_CONFIG core.autocrlf=false
    UPDATE_DISCONNECTED TRUE
    CMAKE_ARGS ${MS_COMMON_ARGS}
        -DBUILD_SHARED_LIBS=OFF -DOP_DISABLE_HTTP=ON -DOP_DISABLE_DOCS=ON -DOP_DISABLE_EXAMPLES=ON
)
add_dependencies(opusfile ogg opus)

ms_dep(libzip https://github.com/nih-at/libzip.git v1.11.4
    -DBUILD_SHARED_LIBS=OFF -DBUILD_TOOLS=OFF -DBUILD_REGRESS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOC=OFF
    -DBUILD_OSSFUZZ=OFF -DENABLE_BZIP2=OFF -DENABLE_LZMA=OFF -DENABLE_ZSTD=OFF -DENABLE_OPENSSL=OFF
    -DENABLE_GNUTLS=OFF -DENABLE_MBEDTLS=OFF -DENABLE_COMMONCRYPTO=OFF -DENABLE_WINDOWS_CRYPTO=OFF)

ms_dep(nlohmann_json https://github.com/nlohmann/json.git v3.12.0
    -DJSON_BuildTests=OFF -DJSON_Install=ON)

ms_dep(tinyxml2 https://github.com/leethomason/tinyxml2.git 11.0.0
    -DBUILD_SHARED_LIBS=OFF -Dtinyxml2_BUILD_TESTING=OFF)

ms_dep(spdlog https://github.com/gabime/spdlog.git v1.16.0
    -DSPDLOG_BUILD_SHARED=OFF -DSPDLOG_BUILD_EXAMPLE=OFF -DSPDLOG_BUILD_TESTS=OFF -DSPDLOG_INSTALL=ON)

# 2Ship links ZAPDLib (its in-game ROM extractor), which reads and writes PNGs.
ms_dep(libpng https://github.com/pnggroup/libpng.git v1.6.44
    -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DPNG_EXECUTABLES=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF)
