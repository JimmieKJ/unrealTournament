/************************************************************************************

Filename    :   GlSetup.h
Content     :   GL Setup
Created     :   August 24, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/
#ifndef OVR_GlSetup_h
#define OVR_GlSetup_h

#include "Android/GlUtils.h"

namespace OVR
{

struct eglSetup_t
{
	int			glEsVersion;		// 2 or 3
	GpuType		gpuType;
	EGLDisplay	display;
	EGLSurface	pbufferSurface;		// use to make context current when we don't have window surfaces
	EGLConfig	config;
	EGLContext	context;
};

// Create an appropriate config, a tiny pbuffer surface, and a context,
// then make it current.  This combination can be used before and after
// the actual window surfaces are available.
// egl.context will be EGL_NO_CONTEXT if there was an error.
// requestedGlEsVersion can be 2 or 3.  If 3 is requested, 2 might still
// be returned in eglSetup_t.glEsVersion.
//
// If contextPriority == EGL_CONTEXT_PRIORITY_MID_IMG, then no priority
// attribute will be set, otherwise EGL_CONTEXT_PRIORITY_LEVEL_IMG will
// be set with contextPriority.
eglSetup_t	EglSetup( const EGLContext shareContext,
		const int requestedGlEsVersion,
		const int redBits, const int greenBits, const int blueBits,
		const int depthBits, const int multisamples,
		const GLuint contextPriority );

void	EglShutdown( eglSetup_t & eglr );

}	// namespace OVR

#endif	// OVR_GlSetup_h
