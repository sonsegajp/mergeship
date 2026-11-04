package com.mergeship2.app;

import org.libsdl.app.SDLActivity;

// SDL's activity, running SDL_main in libmain.so (host/main_android.cpp), which loads both games. libultraship.so and
// libc++_shared.so load as libmain.so's dependencies; libsoh.so and lib2ship.so are opened by the host itself.
public class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
