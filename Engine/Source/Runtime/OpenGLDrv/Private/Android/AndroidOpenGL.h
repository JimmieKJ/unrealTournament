// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidOpenGL.h: Public OpenGL ES definitions for Android-specific functionality
=============================================================================*/
#pragma once


#if PLATFORM_ANDROID
#if !PLATFORM_ANDROIDGL4
#include "AndroidEGL.h"

#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GLES2/gl2ext.h>
	
typedef EGLSyncKHR UGLsync;
#define GLdouble		GLfloat
typedef khronos_int64_t GLint64;
typedef khronos_uint64_t GLuint64;
#define GL_CLAMP		GL_CLAMP_TO_EDGE

#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY	GL_WRITE_ONLY_OES
#endif

#define glTexEnvi(...)

#ifndef GL_RGBA8
#define GL_RGBA8		GL_RGBA // or GL_RGBA8_OES ?
#endif

#define GL_BGRA			GL_BGRA_EXT 
#define GL_UNSIGNED_INT_8_8_8_8_REV	GL_UNSIGNED_BYTE
#define glMapBuffer		glMapBufferOES
#define glUnmapBuffer	glUnmapBufferOES

#ifndef GL_HALF_FLOAT
#define GL_HALF_FLOAT	GL_HALF_FLOAT_OES
#endif

#define GL_COMPRESSED_RGB8_ETC2           0x9274
#define GL_COMPRESSED_SRGB8_ETC2          0x9275
#define GL_COMPRESSED_RGBA8_ETC2_EAC      0x9278
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC 0x9279

#define GL_READ_FRAMEBUFFER_NV					0x8CA8
#define GL_DRAW_FRAMEBUFFER_NV					0x8CA9

typedef void (GL_APIENTRYP PFNBLITFRAMEBUFFERNVPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
extern PFNBLITFRAMEBUFFERNVPROC					glBlitFramebufferNV ;

#define GL_QUERY_COUNTER_BITS_EXT				0x8864
#define GL_CURRENT_QUERY_EXT					0x8865
#define GL_QUERY_RESULT_EXT						0x8866
#define GL_QUERY_RESULT_AVAILABLE_EXT			0x8867
#define GL_SAMPLES_PASSED_EXT					0x8914
#define GL_ANY_SAMPLES_PASSED_EXT				0x8C2F

/* from GL_EXT_color_buffer_half_float */
#define GL_RGBA16F_EXT                          0x881A

typedef void (GL_APIENTRYP PFNGLGENQUERIESEXTPROC) (GLsizei n, GLuint *ids);
typedef void (GL_APIENTRYP PFNGLDELETEQUERIESEXTPROC) (GLsizei n, const GLuint *ids);
typedef GLboolean (GL_APIENTRYP PFNGLISQUERYEXTPROC) (GLuint id);
typedef void (GL_APIENTRYP PFNGLBEGINQUERYEXTPROC) (GLenum target, GLuint id);
typedef void (GL_APIENTRYP PFNGLENDQUERYEXTPROC) (GLenum target);
typedef void (GL_APIENTRYP PFNGLQUERYCOUNTEREXTPROC) (GLuint id, GLenum target);
typedef void (GL_APIENTRYP PFNGLGETQUERYIVEXTPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTIVEXTPROC) (GLuint id, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTUIVEXTPROC) (GLuint id, GLenum pname, GLuint *params);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTUI64VEXTPROC) (GLuint id, GLenum pname, GLuint64 *params);
typedef void* (GL_APIENTRYP PFNGLMAPBUFFEROESPROC) (GLenum target, GLenum access);
typedef GLboolean (GL_APIENTRYP PFNGLUNMAPBUFFEROESPROC) (GLenum target);
typedef void (GL_APIENTRYP PFNGLPUSHGROUPMARKEREXTPROC) (GLsizei length, const GLchar *marker);
typedef void (GL_APIENTRYP PFNGLLABELOBJECTEXTPROC) (GLenum type, GLuint object, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP PFNGLGETOBJECTLABELEXTPROC) (GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GL_APIENTRYP PFNGLPOPGROUPMARKEREXTPROC) (void);
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);
typedef void (GL_APIENTRYP PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
/** from ES 3.0 but can be called on certain Adreno devices */
typedef void (GL_APIENTRYP PFNGLTEXSTORAGE2DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);

extern PFNGLGENQUERIESEXTPROC 			glGenQueriesEXT;
extern PFNGLDELETEQUERIESEXTPROC 		glDeleteQueriesEXT;
extern PFNGLISQUERYEXTPROC 				glIsQueryEXT ;
extern PFNGLBEGINQUERYEXTPROC 			glBeginQueryEXT;
extern PFNGLENDQUERYEXTPROC 			glEndQueryEXT;
extern PFNGLQUERYCOUNTEREXTPROC			glQueryCounterEXT;
extern PFNGLGETQUERYIVEXTPROC 			glGetQueryivEXT;  
extern PFNGLGETQUERYOBJECTIVEXTPROC 	glGetQueryObjectivEXT;
extern PFNGLGETQUERYOBJECTUIVEXTPROC 	glGetQueryObjectuivEXT;
extern PFNGLGETQUERYOBJECTUI64VEXTPROC	glGetQueryObjectui64vEXT;
extern PFNGLMAPBUFFEROESPROC			glMapBufferOES;
extern PFNGLUNMAPBUFFEROESPROC			glUnmapBufferOES;
extern PFNGLDISCARDFRAMEBUFFEREXTPROC 	glDiscardFramebufferEXT ;
extern PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC	glFramebufferTexture2DMultisampleEXT;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC	glRenderbufferStorageMultisampleEXT;
extern PFNGLPUSHGROUPMARKEREXTPROC		glPushGroupMarkerEXT;
extern PFNGLLABELOBJECTEXTPROC			glLabelObjectEXT;
extern PFNGLGETOBJECTLABELEXTPROC		glGetObjectLabelEXT;
extern PFNGLPOPGROUPMARKEREXTPROC 		glPopGroupMarkerEXT;
extern PFNGLTEXSTORAGE2DPROC			glTexStorage2D;
extern PFNGLDEBUGMESSAGECONTROLKHRPROC	glDebugMessageControlKHR;
extern PFNGLDEBUGMESSAGEINSERTKHRPROC	glDebugMessageInsertKHR;
extern PFNGLDEBUGMESSAGECALLBACKKHRPROC	glDebugMessageCallbackKHR;
extern PFNGLGETDEBUGMESSAGELOGKHRPROC	glDebugMessageLogKHR;
extern PFNGLGETPOINTERVKHRPROC			glGetPointervKHR;
extern PFNGLPUSHDEBUGGROUPKHRPROC		glPushDebugGroupKHR;
extern PFNGLPOPDEBUGGROUPKHRPROC		glPopDebugGroupKHR;
extern PFNGLOBJECTLABELKHRPROC			glObjectLabelKHR;
extern PFNGLGETOBJECTLABELKHRPROC		glGetObjectLabelKHR;
extern PFNGLOBJECTPTRLABELKHRPROC		glObjectPtrLabelKHR;
extern PFNGLGETOBJECTPTRLABELKHRPROC	glGetObjectPtrLabelKHR;

#include "OpenGLES2.h"


extern "C"
{
	extern PFNEGLGETSYSTEMTIMENVPROC eglGetSystemTimeNV;
	extern PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
	extern PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
	extern PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
}

struct FAndroidOpenGL : public FOpenGLES2
{
	static FORCEINLINE EShaderPlatform GetShaderPlatform()
	{
		return SP_OPENGL_ES2_ANDROID;
	}

	// Optional:
	static FORCEINLINE void QueryTimestampCounter(GLuint QueryID)
	{
		glQueryCounterEXT(QueryID, GL_TIMESTAMP_EXT);
	}

	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint *OutResult)
	{
		GLenum QueryName = (QueryMode == QM_Result) ? GL_QUERY_RESULT_EXT : GL_QUERY_RESULT_AVAILABLE_EXT;
		glGetQueryObjectuivEXT(QueryId, QueryName, OutResult);
	}

	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint64* OutResult)
	{
		GLenum QueryName = (QueryMode == QM_Result) ? GL_QUERY_RESULT_EXT : GL_QUERY_RESULT_AVAILABLE_EXT;
		GLuint64 Result = 0;
		glGetQueryObjectui64vEXT(QueryId, QueryName, &Result);
		*OutResult = Result;
	}

	static FORCEINLINE void DeleteSync(UGLsync Sync)
	{
		if (GUseThreadedRendering)
		{
			//handle error here
			EGLBoolean Result = eglDestroySyncKHR( AndroidEGL::GetInstance()->GetDisplay(), Sync );
			if(Result == EGL_FALSE)
			{
				//handle error here
			}
		}
	}

	static FORCEINLINE UGLsync FenceSync(GLenum Condition, GLbitfield Flags)
	{
		check(Condition == GL_SYNC_GPU_COMMANDS_COMPLETE && Flags == 0);
		return GUseThreadedRendering ? eglCreateSyncKHR( AndroidEGL::GetInstance()->GetDisplay(), EGL_SYNC_FENCE_KHR, NULL ) : 0;
	}
	
	static FORCEINLINE bool IsSync(UGLsync Sync)
	{
		if(GUseThreadedRendering)
		{
			return (Sync != EGL_NO_SYNC_KHR) ? true : false;
		}
		else
		{
			return true;
		}
	}

	static FORCEINLINE EFenceResult ClientWaitSync(UGLsync Sync, GLbitfield Flags, GLuint64 Timeout)
	{
		if (GUseThreadedRendering)
		{
			// check( Flags == GL_SYNC_FLUSH_COMMANDS_BIT );
			GLenum Result = eglClientWaitSyncKHR( AndroidEGL::GetInstance()->GetDisplay(), Sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, Timeout );
			switch (Result)
			{
			case EGL_TIMEOUT_EXPIRED_KHR:		return FR_TimeoutExpired;
			case EGL_CONDITION_SATISFIED_KHR:	return FR_ConditionSatisfied;
			}
			return FR_WaitFailed;
		}
		else
		{
			return FR_ConditionSatisfied;
		}
		return FR_WaitFailed;
	}
	

	// Required:
	static FORCEINLINE void BlitFramebuffer(GLint SrcX0, GLint SrcY0, GLint SrcX1, GLint SrcY1, GLint DstX0, GLint DstY0, GLint DstX1, GLint DstY1, GLbitfield Mask, GLenum Filter)
	{
		if(glBlitFramebufferNV)
		{
			glBlitFramebufferNV(SrcX0, SrcY0, SrcX1, SrcY1, DstX0, DstY0, DstX1, DstY1, Mask, Filter);
		}
	}

	static FORCEINLINE bool TexStorage2D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLenum Format, GLenum Type, uint32 Flags)
	{
		if( bUseHalfFloatTexStorage && Type == GL_HALF_FLOAT_OES && (Flags & TexCreate_RenderTargetable) != 0 )
		{
			glTexStorage2D(Target, Levels, InternalFormat, Width, Height);
			VERIFY_GL(glTexStorage2D)
			return true;
		}
		else
		{
			return false;
		}
	}

	// Adreno doesn't support HALF_FLOAT
	static FORCEINLINE int32 GetReadHalfFloatPixelsEnum()				{ return GL_FLOAT; }

	// Android ES2 shaders have code that allows compile selection of
	// 32 bpp HDR encoding mode via 'intrinsic_GetHDR32bppEncodeModeES2()'.
	static FORCEINLINE bool SupportsHDR32bppEncodeModeIntrinsic()		{ return true; }

	static FORCEINLINE bool UseES30ShadingLanguage()
	{
		return bUseES30ShadingLanguage;
	}

	static void ProcessExtensions(const FString& ExtensionsString);

	// whether to use ES 3.0 function glTexStorage2D to allocate storage for GL_HALF_FLOAT_OES render target textures
	static bool bUseHalfFloatTexStorage;

	// whether to use ES 3.0 shading language
	static bool bUseES30ShadingLanguage;
};

typedef FAndroidOpenGL FOpenGL;


/** Unreal tokens that maps to different OpenGL tokens by platform. */
#undef UGL_DRAW_FRAMEBUFFER
#define UGL_DRAW_FRAMEBUFFER	GL_DRAW_FRAMEBUFFER_NV
#undef UGL_READ_FRAMEBUFFER
#define UGL_READ_FRAMEBUFFER	GL_READ_FRAMEBUFFER_NV

#else // !PLATFORM_ANDROIDGL4

#include "../AndroidGL4/AndroidGL4OpenGL.h"

#endif

#endif // PLATFORM_ANDROID
