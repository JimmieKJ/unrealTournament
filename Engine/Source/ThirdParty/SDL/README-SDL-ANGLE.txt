1 - Download SDL 1.2.15
2 - put SDL_angle.c SDL_angle_c.h in SDL-1.2.15\src\video\wincommon
3 - open SDL-1.2.15\VisualC\SDL.sln
4 - remove SDL_wingl.c and SDL_wingl_c.h from project, add the angle.c file
5 - build
6 - copy x86/x64 DLLs to lib/x64 and lib/x86
