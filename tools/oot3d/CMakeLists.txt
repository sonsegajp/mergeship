# oot3d-extract: converts a decrypted Ocarina of Time 3D ROM into a Ship of Harkinian mod (.o2r).
# Built on its own (tools/oot3d/build.ps1) against the host build's vcpkg (build/vcpkg), so testing the converter never
# touches a game build. The sources in src/ have no SoH dependencies, so soh.dll can compile them later for extraction
# at runtime.
cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(oot3d_extract LANGUAGES C CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
find_package(ZLIB REQUIRED)

file(GLOB OOT3D_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
add_library(oot3d STATIC ${OOT3D_SOURCES})
# The engine's character code (skinning, 3DS animation sampling) is shared, so offline renders match the game.
target_include_directories(oot3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
                           ${CMAKE_CURRENT_SOURCE_DIR}/../../Shipwright/soh/soh/MergeShip)
target_link_libraries(oot3d PUBLIC ZLIB::ZLIB)

add_executable(oot3d-extract cli/main.cpp)
target_link_libraries(oot3d-extract PRIVATE oot3d)

if(MSVC)
    # The same warning level SoH builds with, so these files compile unchanged in soh.dll later.
    foreach(t oot3d oot3d-extract)
        target_compile_options(${t} PRIVATE /W3 /WX /utf-8 /MP /permissive- /Zc:preprocessor)
        target_compile_definitions(${t} PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
    endforeach()
endif()
