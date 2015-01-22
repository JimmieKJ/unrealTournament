/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/* WGL implementation of SDL OpenGL support using ANGLE */

#include "../SDL_sysvideo.h"
#include "EGL/egl.h"

#ifndef SDL_VIDEO_OPENGL
#error must have SDL_VIDEO_OPENGL defined
#endif

struct SDL_PrivateGLData {
    EGLDisplay EGL_display;
    EGLContext EGL_context;
    EGLSurface EGL_surface;
};

/* OpenGL functions */
extern int WIN_GL_SetupWindow(_THIS);
extern void WIN_GL_ShutDown(_THIS);
extern int WIN_GL_MakeCurrent(_THIS);
extern int WIN_GL_GetAttribute(_THIS, SDL_GLattr attrib, int* value);
extern void WIN_GL_SwapBuffers(_THIS);
extern void WIN_GL_UnloadLibrary(_THIS);
extern int WIN_GL_LoadLibrary(_THIS, const char* path);
extern void *WIN_GL_GetProcAddress(_THIS, const char* proc);
