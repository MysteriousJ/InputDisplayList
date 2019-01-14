@echo off
set SDL_INCLUDE="../sdl/include"
set SDL_LIB="../sdl/lib"

set params=-Zi /EHsc /MT
set libs="SDL2.lib" "SDL2main.lib" "opengl32.lib" "glu32.lib" "kernel32.lib" "user32.lib" "gdi32.lib" "Dwmapi.lib"
cl %params% /D"WIN32" /D"ENABLE_TRANSPARENCY" /I"%SDL_INCLUDE%" "src/main.cpp" /link -subsystem:windows %libs% /LIBPATH:"%SDL_LIB%" /OUT:"InputDisplayList.exe"