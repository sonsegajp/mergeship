# MergeShip2 hosted builds: every module uses the shared /MD CRT, and every
# dependency is linked statically -- except SDL2, which must be one DLL shared by
# libultraship.dll, soh.dll, 2ship.dll and mergeship.exe (SoH calls SDL's joystick
# API directly, and SDL keeps process-wide state).
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
if(PORT STREQUAL "sdl2")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
