/************************************************************************************

Filename    :   GlSetup.cpp
Content     :   GL Setup
Created     :   August 24, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "GlSetup.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include "Android/LogUtils.h"

// TODO: remove when new glext is available
#define EGL_OPENGL_ES3_BIT_KHR      0x0040

namespace OVR {

static void LogStringWords( const char * allExtensions )
{
	const char * start = allExtensions;
	while( 1 )
	{
		const char * end = strstr( start, " " );
		if ( end == NULL )
		{
			break;
		}
		unsigned int nameLen = (unsigned int)(end - start);
		if ( nameLen > 256 )
		{
			nameLen = 256;
		}
		char * word = new char[nameLen+1];
		memcpy( word, start, nameLen );
		word[nameLen] = '\0';
		LOG( "%s", word );
		delete[] word;

		start = end + 1;
	}
}

void DumpEglConfigs( const EGLDisplay display )
{
	static const int MAX_CONFIGS = 1024;
	EGLConfig 	configs[MAX_CONFIGS];
	EGLint  	numConfigs = 0;

	if ( EGL_FALSE == eglGetConfigs( display,
			configs, MAX_CONFIGS, &numConfigs ) )
	{
		WARN( "eglGetConfigs() failed" );
		return;
	}

	LOG( "ES2 configs:" );
	LOG( "  Config R G B A DP S M W P REND" );
	for ( int i = 0; i < numConfigs; i++ )
	{
		EGLint	red = 0;
		eglGetConfigAttrib( display, configs[i], EGL_RED_SIZE, &red );
		EGLint	green = 0;
		eglGetConfigAttrib( display, configs[i], EGL_GREEN_SIZE, &green );
		EGLint	blue = 0;
		eglGetConfigAttrib( display, configs[i], EGL_BLUE_SIZE, &blue );
		EGLint	alpha = 0;
		eglGetConfigAttrib( display, configs[i], EGL_ALPHA_SIZE, &alpha );
		EGLint	depth = 0;
		eglGetConfigAttrib( display, configs[i], EGL_DEPTH_SIZE, &depth );
		EGLint	stencil = 0;
		eglGetConfigAttrib( display, configs[i], EGL_STENCIL_SIZE, &stencil );
		EGLint	multisamples = 0;
		eglGetConfigAttrib( display, configs[i], EGL_SAMPLES, &multisamples );

		// EGL_SURFACE_TYPE is a bit field
		EGLint	surface = 0;
		eglGetConfigAttrib( display, configs[i], EGL_SURFACE_TYPE , &surface );
		EGLint window = (surface & EGL_WINDOW_BIT) != 0;
		EGLint pbuffer = (surface & EGL_PBUFFER_BIT) != 0;

		// EGL_RENDERABLE_TYPE is a bit field
		EGLint	renderable = 0;
		eglGetConfigAttrib( display, configs[i], EGL_RENDERABLE_TYPE , &renderable );
//		EGLint window = (surface & EGL_WINDOW_BIT) != 0;
//		EGLint pbuffer = (surface & EGL_PBUFFER_BIT) != 0;

		LOG( "%8i %i %i %i %i %2i %i %i %i %i 0x%02x 0x%02x", (int)configs[i], red, green, blue, alpha,
				depth, stencil, multisamples, window, pbuffer, renderable, surface);
	}
}

// Returns NULL if no config is found.
EGLConfig ChooseColorConfig( const EGLDisplay display, const int redBits,
		const int greeBits, const int blueBits, const int depthBits, const int samples, const bool pbuffer )
{

	// DumpEglConfigs( display );

	// We do NOT want to use eglChooseConfig, because the Android EGL code pushes in
	// multisample flags behind our back if the user has selected the "force 4x MSAA"
	// option in settings, and that is completely wasted for our warp target.
	static const int MAX_CONFIGS = 1024;
	EGLConfig 	configs[MAX_CONFIGS];
	EGLint  	numConfigs = 0;

	if ( EGL_FALSE == eglGetConfigs( display,
			configs, MAX_CONFIGS, &numConfigs ) ) {
		WARN( "eglGetConfigs() failed" );
		return NULL;
	}
	LOG( "eglGetConfigs() = %i configs", numConfigs );

	// We don't want a depth/stencil buffer
	const EGLint configAttribs[] = {
			EGL_BLUE_SIZE,  	blueBits,
			EGL_GREEN_SIZE, 	greeBits,
			EGL_RED_SIZE,   	redBits,
			EGL_DEPTH_SIZE,   	depthBits,
//			EGL_STENCIL_SIZE,  	0,
			EGL_SAMPLES,		samples,
			EGL_NONE
	};

	// look for OpenGL ES 3.0 configs first, then fall back to 2.0
	for ( int esVersion = 3 ; esVersion >= 2 ; esVersion-- )
	{
		for ( int i = 0; i < numConfigs; i++ )
		{
			EGLint	value = 0;

			// EGL_RENDERABLE_TYPE is a bit field
			eglGetConfigAttrib( display, configs[i], EGL_RENDERABLE_TYPE, &value );

			if ( ( esVersion == 2 ) && ( value & EGL_OPENGL_ES2_BIT ) != EGL_OPENGL_ES2_BIT )
			{
				continue;
			}
			if ( ( esVersion == 3 ) && ( value & EGL_OPENGL_ES3_BIT_KHR ) != EGL_OPENGL_ES3_BIT_KHR )
			{
				continue;
			}

			// For our purposes, the pbuffer config also needs to be compatible with
			// normal window rendering so it can share textures with the window context.
			// I am unsure if it would be portable to always request EGL_PBUFFER_BIT, so
			// I only do it on request.
			eglGetConfigAttrib( display, configs[i], EGL_SURFACE_TYPE , &value );
			const int surfs = EGL_WINDOW_BIT | ( pbuffer ? EGL_PBUFFER_BIT : 0 );
			if ( ( value & surfs ) != surfs )
			{
				continue;
			}

			int	j = 0;
			for ( ; configAttribs[j] != EGL_NONE ; j += 2 )
			{
				EGLint	value = 0;
				eglGetConfigAttrib( display, configs[i], configAttribs[j], &value );
				if ( value != configAttribs[j+1] )
				{
					break;
				}
			}
			if ( configAttribs[j] == EGL_NONE )
			{
				LOG( "Got an ES %i renderable config: %i", esVersion, (int)configs[i] );
				return configs[i];
			}
		}
	}
	return NULL;
}

eglSetup_t EglSetup( const EGLContext shareContext,
		const int requestedGlEsVersion,
		const int redBits, const int greenBits, const int blueBits,
		const int depthBits, const int multisamples, const GLuint contextPriority )
{
	LOG( "EglSetup: requestGlEsVersion(%d), redBits(%d), greenBits(%d), blueBits(%d), depthBits(%d), multisamples(%d), contextPriority(%d)",
			requestedGlEsVersion, redBits, greenBits, blueBits, depthBits, multisamples, contextPriority );

	eglSetup_t egl = {};

	// Get the built in display
	// TODO: check for external HDMI displays
	egl.display = eglGetDisplay( EGL_DEFAULT_DISPLAY );

	// Initialize EGL
	EGLint majorVersion;
	EGLint minorVersion;
	eglInitialize( egl.display, &majorVersion, &minorVersion );
	LOG( "eglInitialize gives majorVersion %i, minorVersion %i", majorVersion, minorVersion);

	const char * eglVendorString = eglQueryString( egl.display, EGL_VENDOR );
	LOG( "EGL_VENDOR: %s", eglVendorString );
	const char * eglClientApisString = eglQueryString( egl.display, EGL_CLIENT_APIS );
	LOG( "EGL_CLIENT_APIS: %s", eglClientApisString );
	const char * eglVersionString = eglQueryString( egl.display, EGL_VERSION );
	LOG( "EGL_VERSION: %s", eglVersionString );
	const char * eglExtensionString = eglQueryString( egl.display, EGL_EXTENSIONS );
	LOG( "EGL_EXTENSIONS:" );
	LogStringWords( eglExtensionString );

	// We do NOT want to use eglChooseConfig, because the Android EGL code pushes in
	// multisample flags behind our back if the user has selected the "force 4x MSAA"
	// option in developer settings, and that is completely wasted for our warp target.
	egl.config = ChooseColorConfig( egl.display, redBits, greenBits, blueBits, depthBits, multisamples, true /* pBuffer compatible */ );
	if ( egl.config == 0 )
	{
		FAIL( "No acceptable EGL color configs." );
		return egl;
	}

	// The EGLContext is created with the EGLConfig
	// Try to get an OpenGL ES 3.0 context first, which is required to do
	// MSAA to framebuffer objects on Adreno.
	for ( int version = requestedGlEsVersion ; version >= 2 ; version-- )
	{
		LOG( "Trying for a EGL_CONTEXT_CLIENT_VERSION %i context shared with %p:",
				version, shareContext );
		// We want the application context to be lower priority than the TimeWarp context.
		EGLint contextAttribs[] = {
				EGL_CONTEXT_CLIENT_VERSION, version,
				EGL_NONE, EGL_NONE,
				EGL_NONE };

		// Don't set EGL_CONTEXT_PRIORITY_LEVEL_IMG at all if set to EGL_CONTEXT_PRIORITY_MEDIUM_IMG,
		// It is the caller's responsibility to use that if the driver doesn't support it.
		if ( contextPriority != EGL_CONTEXT_PRIORITY_MEDIUM_IMG )
		{
			contextAttribs[2] = EGL_CONTEXT_PRIORITY_LEVEL_IMG;
			contextAttribs[3] = contextPriority;
		}

		egl.context = eglCreateContext( egl.display, egl.config, shareContext, contextAttribs );
		if ( egl.context != EGL_NO_CONTEXT )
		{
			LOG( "Succeeded." );
			egl.glEsVersion = version;

			EGLint configIDReadback;
			if ( !eglQueryContext( egl.display, egl.context, EGL_CONFIG_ID, &configIDReadback ) )
			{
			     WARN("eglQueryContext EGL_CONFIG_ID failed" );
			}
			EGLConfig configCheck = EglConfigForConfigID( egl.display, configIDReadback );

			LOG( "Created context with config %i, query returned ID %i = config %i",
					(int)egl.config, configIDReadback, (int)configCheck );
			break;
		}
	}
	if ( egl.context == EGL_NO_CONTEXT )
	{
		WARN( "eglCreateContext failed: %s", EglErrorString() );
		return egl;
	}

	if ( contextPriority != EGL_CONTEXT_PRIORITY_MEDIUM_IMG )
	{
		// See what context priority we actually got
		EGLint actualPriorityLevel;
		eglQueryContext( egl.display, egl.context, EGL_CONTEXT_PRIORITY_LEVEL_IMG, &actualPriorityLevel );
		switch ( actualPriorityLevel )
		{
			case EGL_CONTEXT_PRIORITY_HIGH_IMG: LOG( "Context is EGL_CONTEXT_PRIORITY_HIGH_IMG" ); break;
			case EGL_CONTEXT_PRIORITY_MEDIUM_IMG: LOG( "Context is EGL_CONTEXT_PRIORITY_MEDIUM_IMG" ); break;
			case EGL_CONTEXT_PRIORITY_LOW_IMG: LOG( "Context is EGL_CONTEXT_PRIORITY_LOW_IMG" ); break;
			default: LOG( "Context has unknown priority level" ); break;
		}
	}

	// Because EGL_KHR_surfaceless_context is not widespread (Only on Tegra as of
	// September 2013), we need to create a tiny pbuffer surface to make the context
	// current.
	//
	// It is necessary to use a config with the same characteristics that the
	// context was created with, plus the pbuffer flag, or we will get an
	// EGL_BAD_MATCH error on the eglMakeCurrent() call.
	//
	// This is necessary to support 565 framebuffers, which may be important
	// for higher refresh rate displays.
	const EGLint attrib_list[] =
	{
		EGL_WIDTH, 16,
		EGL_HEIGHT, 16,
		EGL_NONE
	};
	egl.pbufferSurface = eglCreatePbufferSurface( egl.display, egl.config, attrib_list );
	if ( egl.pbufferSurface == EGL_NO_SURFACE )
	{
		WARN( "eglCreatePbufferSurface failed: %s", EglErrorString() );
		eglDestroyContext( egl.display, egl.context );
		egl.context = EGL_NO_CONTEXT;
		return egl;
	}

	if ( eglMakeCurrent( egl.display, egl.pbufferSurface, egl.pbufferSurface, egl.context ) == EGL_FALSE )
	{
		WARN( "eglMakeCurrent pbuffer failed: %s", EglErrorString() );
		eglDestroySurface( egl.display, egl.pbufferSurface );
		eglDestroyContext( egl.display, egl.context );
		egl.context = EGL_NO_CONTEXT;
		return egl;
	}

	const char * glVendorString = (const char *) glGetString(GL_VENDOR);
	LOG( "GL_VENDOR: %s", glVendorString);
	const char * glRendererString = (const char *) glGetString(GL_RENDERER);
	LOG( "GL_RENDERER: %s", glRendererString);
	const char * glVersionString = (const char *) glGetString(GL_VERSION);
	LOG( "GL_VERSION: %s", glVersionString);
	const char * glSlVersionString = (const char *) glGetString(
			GL_SHADING_LANGUAGE_VERSION);
	LOG( "GL_SHADING_LANGUAGE_VERSION: %s", glSlVersionString);

	egl.gpuType = EglGetGpuType();

	return egl;
}

void EglShutdown( eglSetup_t & eglr )
{
	if ( eglMakeCurrent( eglr.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ) == EGL_FALSE )
	{
		FAIL( "eglMakeCurrent: failed: %s", EglErrorString() );
	}
	if ( eglDestroyContext( eglr.display, eglr.context ) == EGL_FALSE )
	{
		FAIL( "eglDestroyContext: failed: %s", EglErrorString() );
	}
	if ( eglDestroySurface( eglr.display, eglr.pbufferSurface ) == EGL_FALSE )
	{
		FAIL( "eglDestroySurface: failed: %s", EglErrorString() );
	}

	eglr.glEsVersion = 0;
	eglr.gpuType = GPU_TYPE_UNKNOWN;
	eglr.display = 0;
	eglr.pbufferSurface = 0;
	eglr.config = 0;
	eglr.context = 0;
}

}	// namespace OVR
