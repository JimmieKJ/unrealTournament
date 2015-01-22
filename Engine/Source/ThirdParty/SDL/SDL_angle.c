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

/* WGL implementation of SDL OpenGL support */

#include "SDL_lowvideo.h"
#include "SDL_angle_c.h"

int WIN_GL_SetupWindow(_THIS)
{
    EGLint numConfigs;
    EGLint majorVersion;
    EGLint minorVersion;
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    EGLConfig config;
    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
    EGLint configAttribList[] = {
        EGL_RED_SIZE,       8,
        EGL_GREEN_SIZE,     8,
        EGL_BLUE_SIZE,      8,
        EGL_ALPHA_SIZE,     EGL_DONT_CARE,
        EGL_DEPTH_SIZE,     8,
        EGL_STENCIL_SIZE,   8,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_NONE
    };
    EGLint surfaceAttribList[] = {
        EGL_NONE, EGL_NONE
    };

    HDC sdldc = GetDC(SDL_Window);

    // Get Display
    display = eglGetDisplay(sdldc);
    if (display == EGL_NO_DISPLAY)
        return -1;

    // Initialize EGL
    if (!eglInitialize(display, &majorVersion, &minorVersion))
        return -1;

    // Figure out a config
    if (!eglGetConfigs(display, NULL, 0, &numConfigs))
        return -1;

    if (!eglChooseConfig(display, configAttribList, &config, 1, &numConfigs))
        return -1;

    surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)SDL_Window, surfaceAttribList);
    if (surface == EGL_NO_SURFACE)
        return -1;

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT)
        return -1;

    if (!eglMakeCurrent(display, surface, surface, context))
        return -1;

    this->gl_data->EGL_display = display;
    this->gl_data->EGL_context = context;
    this->gl_data->EGL_surface = surface;

    // we'll be statically linked
    this->gl_config.driver_loaded = 1;

    return 0;
}

void WIN_GL_ShutDown(_THIS)
{
    // yeah, whatever
}

/* Make the current context active */
int WIN_GL_MakeCurrent(_THIS)
{
    if (!eglMakeCurrent(this->gl_data->EGL_display,
                        this->gl_data->EGL_surface,
                        this->gl_data->EGL_surface,
                        this->gl_data->EGL_context))
        return -1;
    return 0;
}

/* Get attribute data from wgl. */
int WIN_GL_GetAttribute(_THIS, SDL_GLattr attrib, int* value)
{
    return -1;
}

void WIN_GL_SwapBuffers(_THIS)
{
    eglSwapBuffers(this->gl_data->EGL_display, this->gl_data->EGL_surface);
}

void WIN_GL_UnloadLibrary(_THIS)
{
}

/* Passing a NULL path means load pointers from the application */
int WIN_GL_LoadLibrary(_THIS, const char* path) 
{
	return 0;
}

void *WIN_GL_GetProcAddress(_THIS, const char* proc)
{
    return eglGetProcAddress(proc);
}
