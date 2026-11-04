# MergeShip2 host: libultraship.dll (from the libultraship fork, branch mergeship2) and mergeship.exe,
# which loads soh.dll (Shipwright, -DMERGESHIP_HOSTED=ON) and 2ship.dll (2ship2harkinian, same option)
# into one process and one window. Built by tools/build_host.ps1; see
# Shipwright/mergeship2/docs/HOST_DESIGN.md.
#
# The three trees (this one, SoH's and 2Ship's) are separate CMake builds because their target names
# collide. Each game tree compiles libultraship again from its own submodule checkout (same commit, checked
# by build_host.ps1) for its import library and headers; the DLL that ships is this tree's, and the
# engines refuse to run against a DLL whose LUS_GetAbiId() differs from what they were built against.
cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)

if(NOT ANDROID_ABI) # (Android's toolchain sets it to the API level)
    set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)
endif()
set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_VS_PLATFORM_NAME)
    set(CMAKE_VS_PLATFORM_NAME "x64")
endif()

project(mergeship LANGUAGES C CXX)

################################################################################
# Android (android/build_android.ps1): libultraship.so and libmain.so, which SDL's Java activity loads and which
# loads libsoh.so and lib2ship.so (main_android.cpp). The dependencies come from android/deps' prefix.
################################################################################
if(ANDROID)
    set(MERGESHIP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "MergeShip2 repository root")
    set(MERGESHIP_LUS_SOURCE_DIR "${MERGESHIP_ROOT}/Shipwright/libultraship" CACHE PATH "libultraship fork")
    include(${MERGESHIP_ROOT}/Shipwright/CMake/soh-cvars.cmake)
    file(READ ${MERGESHIP_ROOT}/Shipwright/CMake/lus-cvars.cmake _lus_cvars)
    string(REGEX REPLACE "include\\([^)]*\\)" "" _lus_cvars "${_lus_cvars}")
    cmake_language(EVAL CODE "${_lus_cvars}")

    set(SPDLOG_LEVEL_TRACE 0)
    set(SPDLOG_LEVEL_OFF 6)
    set(SPDLOG_MIN_CUTOFF SPDLOG_LEVEL_TRACE CACHE STRING "cutoff at trace")
    set(GFX_DEBUG_DISASSEMBLER ON)
    set(GBI_UCODE F3DEX_GBI_2)
    set(INCLUDE_MPQ_SUPPORT ON)
    add_compile_definitions(CONTROLLERBUTTONS_T=uint32_t)

    set(USE_OPENGLES ON CACHE BOOL "" FORCE)
    set(LUS_SHARED ON CACHE BOOL "" FORCE)
    add_subdirectory(${MERGESHIP_LUS_SOURCE_DIR} ${CMAKE_BINARY_DIR}/libultraship SYSTEM)
    target_compile_definitions(libultraship PUBLIC INCLUDE_MPQ_SUPPORT)
    target_compile_options(libultraship PRIVATE -w)

    find_package(SDL2 REQUIRED)
    add_library(main SHARED main_android.cpp EngineModule.cpp NetService.cpp)
    target_include_directories(main PRIVATE ${MERGESHIP_ROOT}/include)
    target_link_libraries(main PRIVATE libultraship SDL2::SDL2 log android)
    message(STATUS "MergeShip2 Android host: libultraship ABI ${LUS_ABI_ID}")
    return()
endif()

if(NOT MSVC)
    message(FATAL_ERROR "The MergeShip2 host is Windows/MSVC or Android only for now.")
endif()

set(MERGESHIP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "MergeShip2 repository root")
set(MERGESHIP_LUS_SOURCE_DIR "${MERGESHIP_ROOT}/Shipwright/libultraship" CACHE PATH
    "libultraship fork checkout (branch mergeship2) to build libultraship.dll from")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
add_compile_options(/MP /utf-8 /Zc:preprocessor /external:W0 /fp:precise /fp:except-)

################################################################################
# libultraship, configured exactly as Shipwright's top-level CMakeLists configures it
################################################################################
include(${MERGESHIP_ROOT}/Shipwright/CMake/soh-cvars.cmake)
# Shipwright/CMake/lus-cvars.cmake without its trailing include(), which is relative to Shipwright/.
file(READ ${MERGESHIP_ROOT}/Shipwright/CMake/lus-cvars.cmake _lus_cvars)
string(REGEX REPLACE "include\\([^)]*\\)" "" _lus_cvars "${_lus_cvars}")
cmake_language(EVAL CODE "${_lus_cvars}")

set(SPDLOG_LEVEL_TRACE 0)
set(SPDLOG_LEVEL_OFF 6)
set(SPDLOG_MIN_CUTOFF SPDLOG_LEVEL_TRACE CACHE STRING "cutoff at trace")
set(GFX_DEBUG_DISASSEMBLER ON)
set(GBI_UCODE F3DEX_GBI_2)
set(INCLUDE_MPQ_SUPPORT ON)
add_compile_definitions(CONTROLLERBUTTONS_T=uint32_t)

set(LUS_SHARED ON CACHE BOOL "" FORCE)
add_subdirectory(${MERGESHIP_LUS_SOURCE_DIR} ${CMAKE_BINARY_DIR}/libultraship SYSTEM)
target_compile_definitions(libultraship PUBLIC INCLUDE_MPQ_SUPPORT)
target_compile_options(libultraship PRIVATE /w)

################################################################################
# mergeship.exe
################################################################################
add_executable(mergeship
    main.cpp
    EngineModule.cpp
    EngineModule.h
    FrameRecorder.cpp
    FrameRecorder.h
    NetService.cpp
    NetService.h
    Screenshot.cpp
    Screenshot.h
)
target_include_directories(mergeship PRIVATE ${MERGESHIP_ROOT}/include)
target_link_libraries(mergeship PRIVATE libultraship user32 gdi32 ws2_32)
target_compile_definitions(mergeship PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS
                                             SDL_MAIN_HANDLED)
# The games run on this thread. Their static tables (e.g. SoH's randomizer location lists) are built in functions
# with very large frames; 16 MB, as SoH reserves for its ASAN builds, instead of the default 1 MB.
target_link_options(mergeship PRIVATE /SUBSYSTEM:CONSOLE /DEBUG /STACK:16777216)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mergeship)

################################################################################
# Keep every output in this build tree (libultraship's use_props would write <source>/x64/<config>)
################################################################################
function(mergeship_outputs_here dir)
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_target IN LISTS _targets)
        get_target_property(_type ${_target} TYPE)
        if(NOT _type STREQUAL "INTERFACE_LIBRARY" AND NOT _type STREQUAL "UTILITY")
            set_target_properties(${_target} PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
            # Some (torch, prism) also spell /MT out in their compile options.
            get_target_property(_options ${_target} COMPILE_OPTIONS)
            if(_options)
                list(TRANSFORM _options REPLACE "/MT" "/MD")
                set_target_properties(${_target} PROPERTIES COMPILE_OPTIONS "${_options}")
            endif()
            foreach(_config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
                string(TOUPPER ${_config} _config_u)
                foreach(_kind IN ITEMS RUNTIME LIBRARY ARCHIVE PDB)
                    set_target_properties(${_target} PROPERTIES
                        ${_kind}_OUTPUT_DIRECTORY_${_config_u} "${CMAKE_BINARY_DIR}/${_config}")
                endforeach()
            endforeach()
        endif()
    endforeach()
    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_subdir IN LISTS _subdirs)
        mergeship_outputs_here("${_subdir}")
    endforeach()
endfunction()
mergeship_outputs_here(${CMAKE_CURRENT_SOURCE_DIR})

message(STATUS "MergeShip2 host: libultraship ABI ${LUS_ABI_ID}")
