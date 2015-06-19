/************************************************************************************

Filename    :   GlUtils.cpp
Content     :   Policy-free OpenGL convenience functions
Created     :   August 24, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "GlUtils.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include "NativeBuildStrings.h"
#include "LogUtils.h"

#if defined(OVR_ENABLE_CAPTURE)
	#include <OVR_Capture.h>
#endif

bool	EXT_discard_framebuffer;
PFNGLDISCARDFRAMEBUFFEREXTPROC glDiscardFramebufferEXT_;

bool	IMG_multisampled_render_to_texture;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMG glRenderbufferStorageMultisampleIMG_;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMG glFramebufferTexture2DMultisampleIMG_;

PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR_;
PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR_;
PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR_;
PFNEGLSIGNALSYNCKHRPROC eglSignalSyncKHR_;
PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR_;

// GL_OES_vertex_array_object
bool	OES_vertex_array_object;
PFNGLBINDVERTEXARRAYOESPROC	glBindVertexArrayOES_;
PFNGLDELETEVERTEXARRAYSOESPROC	glDeleteVertexArraysOES_;
PFNGLGENVERTEXARRAYSOESPROC	glGenVertexArraysOES_;
PFNGLISVERTEXARRAYOESPROC	glIsVertexArrayOES_;

// QCOM_tiled_rendering
bool	QCOM_tiled_rendering;
PFNGLSTARTTILINGQCOMPROC	glStartTilingQCOM_;
PFNGLENDTILINGQCOMPROC		glEndTilingQCOM_;

// EXT_disjoint_timer_query
bool	EXT_disjoint_timer_query;

PFNGLGENQUERIESEXTPROC glGenQueriesEXT_;
PFNGLDELETEQUERIESEXTPROC glDeleteQueriesEXT_;
PFNGLISQUERYEXTPROC glIsQueryEXT_;
PFNGLBEGINQUERYEXTPROC glBeginQueryEXT_;
PFNGLENDQUERYEXTPROC glEndQueryEXT_;
PFNGLQUERYCOUNTEREXTPROC glQueryCounterEXT_;
PFNGLGETQUERYIVEXTPROC glGetQueryivEXT_;
PFNGLGETQUERYOBJECTIVEXTPROC glGetQueryObjectivEXT_;
PFNGLGETQUERYOBJECTUIVEXTPROC glGetQueryObjectuivEXT_;
PFNGLGETQUERYOBJECTI64VEXTPROC glGetQueryObjecti64vEXT_;
PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64vEXT_;
PFNGLGETINTEGER64VPROC glGetInteger64v_;

// EXT_texture_filter_anisotropic
bool EXT_texture_filter_anisotropic = false;

// EXT_sRGB_decode
bool HasEXT_sRGB_texture_decode = false;

// ES3 replacements to allow linking against ES2 libs
void           (*glBlitFramebuffer_) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
void           (*glRenderbufferStorageMultisample_) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
void           (*glInvalidateFramebuffer_) (GLenum target, GLsizei numAttachments, const GLenum* attachments);
GLvoid*        (*glMapBufferRange_) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
GLboolean      (*glUnmapBuffer_) (GLenum target);


/*
 * OpenGL convenience functions
 */
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

void *GetExtensionProc( const char * name )
{
	void * ptr = (void *)eglGetProcAddress( name );
	if ( !ptr )
	{
		LOG( "NOT FOUND: %s", name );
	}
	return ptr;
}

void GL_FindExtensions()
{
	// get extension pointers
	const char * extensions = (const char *)glGetString( GL_EXTENSIONS );
	if ( NULL == extensions )
	{
		LOG( "glGetString( GL_EXTENSIONS ) returned NULL" );
		return;
	}

	// Unfortunately, the Android log truncates strings over 1024 bytes long,
	// even if there are \n inside, so log each word in the string separately.
	LOG( "GL_EXTENSIONS:" );
	LogStringWords( extensions );

	const bool es3 = ( strncmp( (const char *)glGetString( GL_VERSION ), "OpenGL ES 3", 11 ) == 0 );
	LOG( "es3 = %s", es3 ? "TRUE" : "FALSE" );

	if ( GL_ExtensionStringPresent( "GL_EXT_discard_framebuffer", extensions ) )
	{
		EXT_discard_framebuffer = true;
		glDiscardFramebufferEXT_ = (PFNGLDISCARDFRAMEBUFFEREXTPROC)GetExtensionProc( "glDiscardFramebufferEXT" );
	}

	if ( GL_ExtensionStringPresent( "GL_IMG_multisampled_render_to_texture", extensions ) )
	{
		IMG_multisampled_render_to_texture = true;
		glRenderbufferStorageMultisampleIMG_ = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMG)GetExtensionProc ( "glRenderbufferStorageMultisampleIMG" );
		glFramebufferTexture2DMultisampleIMG_ = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMG)GetExtensionProc ( "glFramebufferTexture2DMultisampleIMG" );
	}
	else if ( GL_ExtensionStringPresent( "GL_EXT_multisampled_render_to_texture", extensions ) )
	{
		// assign to the same function pointers as the IMG extension
		IMG_multisampled_render_to_texture = true;
		glRenderbufferStorageMultisampleIMG_ = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMG)GetExtensionProc ( "glRenderbufferStorageMultisampleEXT" );
		glFramebufferTexture2DMultisampleIMG_ = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMG)GetExtensionProc ( "glFramebufferTexture2DMultisampleEXT" );
	}

	eglCreateSyncKHR_ = (PFNEGLCREATESYNCKHRPROC)GetExtensionProc( "eglCreateSyncKHR" );
	eglDestroySyncKHR_ = (PFNEGLDESTROYSYNCKHRPROC)GetExtensionProc( "eglDestroySyncKHR" );
	eglClientWaitSyncKHR_ = (PFNEGLCLIENTWAITSYNCKHRPROC)GetExtensionProc( "eglClientWaitSyncKHR" );
	eglSignalSyncKHR_ = (PFNEGLSIGNALSYNCKHRPROC)GetExtensionProc( "eglSignalSyncKHR" );
	eglGetSyncAttribKHR_ = (PFNEGLGETSYNCATTRIBKHRPROC)GetExtensionProc( "eglGetSyncAttribKHR" );

	if ( GL_ExtensionStringPresent( "GL_OES_vertex_array_object", extensions ) )
	{
		OES_vertex_array_object = true;
		glBindVertexArrayOES_ = (PFNGLBINDVERTEXARRAYOESPROC)eglGetProcAddress("glBindVertexArrayOES");
		glDeleteVertexArraysOES_ = (PFNGLDELETEVERTEXARRAYSOESPROC)eglGetProcAddress("glDeleteVertexArraysOES");
		glGenVertexArraysOES_ = (PFNGLGENVERTEXARRAYSOESPROC)eglGetProcAddress("glGenVertexArraysOES");
		glIsVertexArrayOES_ = (PFNGLISVERTEXARRAYOESPROC)eglGetProcAddress("glIsVertexArrayOES");
	}

	if ( GL_ExtensionStringPresent( "GL_QCOM_tiled_rendering", extensions ) )
	{
		QCOM_tiled_rendering = true;
		glStartTilingQCOM_ = (PFNGLSTARTTILINGQCOMPROC)eglGetProcAddress("glStartTilingQCOM");
		glEndTilingQCOM_ = (PFNGLENDTILINGQCOMPROC)eglGetProcAddress("glEndTilingQCOM");
	}

	// Enabling this seems to cause strange problems in Unity
	if ( GL_ExtensionStringPresent( "GL_EXT_disjoint_timer_query", extensions ) )
	{
		EXT_disjoint_timer_query = true;
		glGenQueriesEXT_ = (PFNGLGENQUERIESEXTPROC)eglGetProcAddress("glGenQueriesEXT");
		glDeleteQueriesEXT_ = (PFNGLDELETEQUERIESEXTPROC)eglGetProcAddress("glDeleteQueriesEXT");
		glIsQueryEXT_ = (PFNGLISQUERYEXTPROC)eglGetProcAddress("glIsQueryEXT");
		glBeginQueryEXT_ = (PFNGLBEGINQUERYEXTPROC)eglGetProcAddress("glBeginQueryEXT");
		glEndQueryEXT_ = (PFNGLENDQUERYEXTPROC)eglGetProcAddress("glEndQueryEXT");
		glQueryCounterEXT_ = (PFNGLQUERYCOUNTEREXTPROC)eglGetProcAddress("glQueryCounterEXT");
		glGetQueryivEXT_ = (PFNGLGETQUERYIVEXTPROC)eglGetProcAddress("glGetQueryivEXT");
		glGetQueryObjectivEXT_ = (PFNGLGETQUERYOBJECTIVEXTPROC)eglGetProcAddress("glGetQueryObjectivEXT");
		glGetQueryObjectuivEXT_ = (PFNGLGETQUERYOBJECTUIVEXTPROC)eglGetProcAddress("glGetQueryObjectuivEXT");
		glGetQueryObjecti64vEXT_ = (PFNGLGETQUERYOBJECTI64VEXTPROC)eglGetProcAddress("glGetQueryObjecti64vEXT");
		glGetQueryObjectui64vEXT_  = (PFNGLGETQUERYOBJECTUI64VEXTPROC)eglGetProcAddress("glGetQueryObjectui64vEXT");
		glGetInteger64v_  = (PFNGLGETINTEGER64VPROC)eglGetProcAddress("glGetInteger64v");
	}

	if ( GL_ExtensionStringPresent( "GL_EXT_texture_sRGB_decode", extensions ) )
	{
		HasEXT_sRGB_texture_decode = true;
	}

	if ( GL_ExtensionStringPresent( "GL_EXT_texture_filter_anisotropic", extensions ) )
	{
		EXT_texture_filter_anisotropic = true;
	}

	GLint MaxTextureSize = 0;
	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &MaxTextureSize );
	LOG( "GL_MAX_TEXTURE_SIZE = %d", MaxTextureSize );

	GLint MaxVertexUniformVectors = 0;
	glGetIntegerv( GL_MAX_VERTEX_UNIFORM_VECTORS, &MaxVertexUniformVectors );
	LOG( "GL_MAX_VERTEX_UNIFORM_VECTORS = %d", MaxVertexUniformVectors );

	GLint MaxFragmentUniformVectors = 0;
	glGetIntegerv( GL_MAX_FRAGMENT_UNIFORM_VECTORS, &MaxFragmentUniformVectors );
	LOG( "GL_MAX_FRAGMENT_UNIFORM_VECTORS = %d", MaxFragmentUniformVectors );

	// ES3 functions we need to getprocaddress to allow linking against ES2 lib
	glBlitFramebuffer_  = (PFNGLBLITFRAMEBUFFER_)eglGetProcAddress("glBlitFramebuffer");
	glRenderbufferStorageMultisample_  = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLE_)eglGetProcAddress("glRenderbufferStorageMultisample");
	glInvalidateFramebuffer_  = (PFNGLINVALIDATEFRAMEBUFFER_)eglGetProcAddress("glInvalidateFramebuffer");
	glMapBufferRange_  = (PFNGLMAPBUFFERRANGE_)eglGetProcAddress("glMapBufferRange");
	glUnmapBuffer_  = (PFNGLUNMAPBUFFEROESPROC_)eglGetProcAddress("glUnmapBuffer");
}

bool GL_ExtensionStringPresent( const char * extension, const char * allExtensions )
{
	if ( strstr( allExtensions, extension ) )
	{
		LOG( "Found: %s", extension );
		return true;
	}
	else
	{
		LOG( "Not found: %s", extension );
		return false;
	}
}

EGLint GL_FlushSync( int timeout )
{
#if defined(OVR_ENABLE_CAPTURE)
	OVR_CAPTURE_CPU_ZONE(GL_FlushSync);
#endif
	// if extension not present, return NO_SYNC
	if ( eglCreateSyncKHR_ == NULL )
	{
		return EGL_FALSE;
	}

	EGLDisplay eglDisplay = eglGetCurrentDisplay();

	const EGLSyncKHR sync = eglCreateSyncKHR_( eglDisplay, EGL_SYNC_FENCE_KHR, NULL );
	if ( sync == EGL_NO_SYNC_KHR )
	{
		return EGL_FALSE;
	}

	const EGLint wait = eglClientWaitSyncKHR_( eglDisplay, sync,
							EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, timeout );

	eglDestroySyncKHR_( eglDisplay, sync );

	return wait;
}

void GL_Finish()
{
#if defined(OVR_ENABLE_CAPTURE)
	OVR_CAPTURE_CPU_ZONE(GL_Finish);
#endif
	// Given the common driver "optimization" of ignoring glFinish, we
	// can't run reliably while drawing to the front buffer without
	// the Sync extension.
	if ( eglCreateSyncKHR_ != NULL )
	{
		// 100 milliseconds == 100000000 nanoseconds
		const EGLint wait = GL_FlushSync( 100000000 );
		if ( wait == EGL_TIMEOUT_EXPIRED_KHR )
		{
			LOG( "EGL_TIMEOUT_EXPIRED_KHR" );
		}
		if ( wait == EGL_FALSE )
		{
			LOG( "eglClientWaitSyncKHR returned EGL_FALSE" );
		}
	}
}

void GL_Flush()
{
#if defined(OVR_ENABLE_CAPTURE)
	OVR_CAPTURE_CPU_ZONE(GL_Flush);
#endif
	if ( eglCreateSyncKHR_ != NULL )
	{
		const EGLint wait = GL_FlushSync( 0 );
		if ( wait == EGL_FALSE )
		{
			LOG("eglClientWaitSyncKHR returned EGL_FALSE");
		}
	}

	// Also do a glFlush() so it shows up in logging tools that
	// don't capture eglClientWaitSyncKHR_ calls.
//	glFlush();
}

// This requires the isFBO parameter because GL ES 3.0's glInvalidateFramebuffer() uses
// different attachment values for FBO vs default framebuffer, unlike glDiscardFramebufferEXT()
void GL_InvalidateFramebuffer( const invalidateTarget_t isFBO, const bool colorBuffer, const bool depthBuffer )
{
	const int offset = (int)!colorBuffer;
	const int count = (int)colorBuffer + ((int)depthBuffer)*2;

	const GLenum fboAttachments[3] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
	const GLenum attachments[3] = { GL_COLOR_EXT, GL_DEPTH_EXT, GL_STENCIL_EXT };
	glInvalidateFramebuffer_( GL_FRAMEBUFFER, count, ( isFBO == INV_FBO ? fboAttachments : attachments ) + offset );
}

const char * EglEnumString( const EGLint e )
{
	switch( e )
	{
	case EGL_TIMEOUT_EXPIRED_KHR: return "EGL_TIMEOUT_EXPIRED_KHR";
	case EGL_CONDITION_SATISFIED_KHR: return "EGL_CONDITION_SATISFIED_KHR";
	case EGL_FALSE: return "EGL_FALSE";
	}
	return "<UNKNOWN EGL ENUM>";
}

EGLConfig EglConfigForConfigID( const EGLDisplay display, const GLint configID )
{
	static const int MAX_CONFIGS = 1024;
	EGLConfig 	configs[MAX_CONFIGS];
	EGLint  	numConfigs = 0;

	if ( EGL_FALSE == eglGetConfigs( display,
			configs, MAX_CONFIGS, &numConfigs ) )
	{
		WARN( "eglGetConfigs() failed" );
		return NULL;
	}

	for ( int i = 0; i < numConfigs; i++ )
	{
		EGLint	value = 0;

		eglGetConfigAttrib( display, configs[i], EGL_CONFIG_ID, &value );
		if ( value == configID )
		{
			return configs[i];
		}
	}

	return NULL;
}

const char * EglErrorString()
{
	const EGLint err = eglGetError();
	switch( err )
	{
	case EGL_SUCCESS:			return "EGL_SUCCESS";
	case EGL_NOT_INITIALIZED:	return "EGL_NOT_INITIALIZED";
	case EGL_BAD_ACCESS:		return "EGL_BAD_ACCESS";
	case EGL_BAD_ALLOC:			return "EGL_BAD_ALLOC";
	case EGL_BAD_ATTRIBUTE:		return "EGL_BAD_ATTRIBUTE";
	case EGL_BAD_CONTEXT:		return "EGL_BAD_CONTEXT";
	case EGL_BAD_CONFIG:		return "EGL_BAD_CONFIG";
	case EGL_BAD_CURRENT_SURFACE:return "EGL_BAD_CURRENT_SURFACE";
	case EGL_BAD_DISPLAY:		return "EGL_BAD_DISPLAY";
	case EGL_BAD_SURFACE:		return "EGL_BAD_SURFACE";
	case EGL_BAD_MATCH:			return "EGL_BAD_MATCH";
	case EGL_BAD_PARAMETER:		return "EGL_BAD_PARAMETER";
	case EGL_BAD_NATIVE_PIXMAP:	return "EGL_BAD_NATIVE_PIXMAP";
	case EGL_BAD_NATIVE_WINDOW:	return "EGL_BAD_NATIVE_WINDOW";
	case EGL_CONTEXT_LOST:		return "EGL_CONTEXT_LOST";
	default: return "Unknown egl error code";
	}
}

const char * GL_ErrorForEnum( const GLenum e )
{
	switch( e )
	{
	case GL_NO_ERROR: return "GL_NO_ERROR";
	case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
	case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
	default: return "Unknown gl error code";
	}
}

bool GL_CheckErrors( const char * logTitle )
{
	bool hadError = false;

	// There can be multiple errors that need reporting.
	do
	{
		GLenum err = glGetError();
		if ( err == GL_NO_ERROR )
		{
			break;
		}
		hadError = true;
		WARN( "%s GL Error: %s", logTitle, GL_ErrorForEnum( err ) );
		if ( err == GL_OUT_OF_MEMORY )
		{
			FAIL( "GL_OUT_OF_MEMORY" );
		}
	} while ( 1 );
	return hadError;
}

GpuType EglGetGpuTypeLocal()
{
	GpuType gpuType;
	const char * glRendererString = (const char *)glGetString( GL_RENDERER );
	if ( strstr( glRendererString, "Adreno (TM) 420" ) )
	{
		gpuType = GPU_TYPE_ADRENO_420;
	}
	else if ( strstr( glRendererString, "Adreno (TM) 330" ) )
	{
		gpuType = GPU_TYPE_ADRENO_330;
	}
	else if ( strstr( glRendererString, "Adreno" ) )
	{
		gpuType = GPU_TYPE_ADRENO;
	}
	else if ( strstr( glRendererString, "Mali-T760") )
	{
		const char * hardware = ovr_GetBuildString( BUILDSTR_HARDWARE );
		if ( strcmp( hardware, "universal5433" ) == 0 )
		{
			gpuType = GPU_TYPE_MALI_T760_EXYNOS_5433;
		}
		else if ( strcmp( hardware, "samsungexynos7420" ) == 0 )
		{
			gpuType = GPU_TYPE_MALI_T760_EXYNOS_7420;
		}
		else
		{
			gpuType = GPU_TYPE_MALI_T760;
		}
	}
	else if ( strstr( glRendererString, "Mali" ) )
	{
		gpuType = GPU_TYPE_MALI;
	}
	else
	{
		gpuType = GPU_TYPE_UNKNOWN;
	}

	LOG( "SoC: %s", ovr_GetBuildString( BUILDSTR_HARDWARE ) );
	LOG( "EglGetGpuType: %d", gpuType );

	return gpuType;
}

GpuType EglGetGpuType()
{
	static GpuType type = EglGetGpuTypeLocal();
	return type;
}

}	// namespace OVR
