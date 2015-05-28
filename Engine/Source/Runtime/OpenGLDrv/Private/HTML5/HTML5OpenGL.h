// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5OpenGL.h: Public OpenGL ES 2.0 definitions for HTML5-specific functionality
=============================================================================*/

#pragma once

#define GL_GLEXT_PROTOTYPES 
#include <SDL_opengles2.h>

typedef char UGLsync; 
typedef long long  GLint64;
typedef unsigned long long  GLuint64;

// empty functions. 
#if PLATFORM_HTML5_BROWSER 
void glDeleteQueriesEXT(GLsizei n,  const GLuint * ids);
void glGenQueriesEXT(GLsizei n,  GLuint * ids); 
void glBeginQueryEXT(GLenum target,  GLuint id);
void glEndQueryEXT (GLenum target);
void glGetQueryObjectuivEXT(GLuint id,  GLenum pname,  GLuint * params);
void glLabelObjectEXT(GLenum type, GLuint object, GLsizei length, const GLchar *label);
void glPushGroupMarkerEXT (GLsizei length, const GLchar *marker);
void glPopGroupMarkerEXT (void);
void glGetObjectLabelEXT (GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, GLchar *label);
#endif 

#define GL_BGRA									0x80E1
#define GL_QUERY_COUNTER_BITS_EXT				0x8864
#define GL_CURRENT_QUERY_EXT					0x8865
#define GL_QUERY_RESULT_EXT						0x8866
#define GL_QUERY_RESULT_AVAILABLE_EXT			0x8867
#define GL_SAMPLES_PASSED_EXT					0x8914 
#define GL_ANY_SAMPLES_PASSED_EXT				0x8C2F

#define GL_LUMINANCE8_EXT GL_LUMINANCE  
#define GL_BGRA8_EXT GL_BGRA 
#define GL_ALPHA8_EXT GL_ALPHA 




/** Unreal tokens that maps to different OpenGL tokens by platform. */
#define GL_DRAW_FRAMEBUFFER	GL_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER GL_FRAMEBUFFER

#include "OpenGLES2.h" 

struct FHTML5OpenGL : public FOpenGLES2
{
	static FORCEINLINE bool IsSync(UGLsync Sync)
	{
		return true; 
	}

	static FORCEINLINE EFenceResult ClientWaitSync(UGLsync Sync, GLbitfield Flags, GLuint64 Timeout)
	{
		return FR_ConditionSatisfied;
	}

	static FORCEINLINE void CheckFrameBuffer() 
	{
		GLenum CompleteResult = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (CompleteResult != GL_FRAMEBUFFER_COMPLETE)
		{
			UE_LOG(LogRHI, Warning,TEXT("Framebuffer not complete. Status = 0x%x"), CompleteResult);
		}
	}

	static FORCEINLINE bool SupportsMapBuffer()							{ return false; }
	static FORCEINLINE bool SupportsCombinedDepthStencilAttachment()	{ return bCombinedDepthStencilAttachment; }
	static FORCEINLINE GLenum GetDepthFormat()							{ return GL_DEPTH_COMPONENT; }
	static FORCEINLINE GLenum GetShadowDepthFormat()					{ return GL_DEPTH_COMPONENT; }
	// Optional
	static FORCEINLINE void BeginQuery(GLenum QueryType, GLuint QueryId) UGL_OPTIONAL_VOID
	static FORCEINLINE void EndQuery(GLenum QueryType) UGL_OPTIONAL_VOID
	static FORCEINLINE void LabelObject(GLenum Type, GLuint Object, const ANSICHAR* Name)UGL_OPTIONAL_VOID
	static FORCEINLINE void PushGroupMarker(const ANSICHAR* Name)UGL_OPTIONAL_VOID
	static FORCEINLINE void PopGroupMarker() UGL_OPTIONAL_VOID
	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, uint64 *OutResult) UGL_OPTIONAL_VOID
	static FORCEINLINE void* MapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize, EResourceLockMode LockMode)	UGL_OPTIONAL(NULL)
	static FORCEINLINE void UnmapBuffer(GLenum Type) UGL_OPTIONAL_VOID
	static FORCEINLINE void UnmapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize)UGL_OPTIONAL_VOID
	static FORCEINLINE void GenQueries(GLsizei NumQueries, GLuint* QueryIDs) UGL_OPTIONAL_VOID
	static FORCEINLINE void DeleteQueries(GLsizei NumQueries, const GLuint* QueryIDs )UGL_OPTIONAL_VOID
	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint *OutResult)UGL_OPTIONAL_VOID
	static FORCEINLINE void FramebufferTexture2D(GLenum Target, GLenum Attachment, GLenum TexTarget, GLuint Texture, GLint Level)
	{
		check(Attachment == GL_COLOR_ATTACHMENT0 || Attachment == GL_DEPTH_ATTACHMENT || Attachment == GL_DEPTH_STENCIL_ATTACHMENT);
		glFramebufferTexture2D(Target, Attachment, TexTarget, Texture, Level);
		VERIFY_GL(FramebufferTexture_2D)
	}

	static FORCEINLINE void DiscardFramebufferEXT(GLenum Target, GLsizei NumAttachments, const GLenum* Attachments) UGL_OPTIONAL_VOID
	static void ProcessExtensions(const FString& ExtensionsString);

	static FORCEINLINE EShaderPlatform GetShaderPlatform()
	{
		return SP_OPENGL_ES2_WEBGL;
	}

	static FORCEINLINE FString GetAdapterName() { return TEXT(""); }

	static FORCEINLINE bool UseES30ShadingLanguage() { return false;}

	static FORCEINLINE GLsizei GetLabelObject(GLenum Type, GLuint Object, GLsizei BufferSize, ANSICHAR* OutName)
	{
		return 0;
	}

	static FORCEINLINE int32 GetReadHalfFloatPixelsEnum()				{ return GL_FLOAT; }
protected:
	/** http://www.khronos.org/registry/webgl/extensions/WEBGL_depth_texture/ */
	static bool bCombinedDepthStencilAttachment;
};


typedef FHTML5OpenGL FOpenGL;

