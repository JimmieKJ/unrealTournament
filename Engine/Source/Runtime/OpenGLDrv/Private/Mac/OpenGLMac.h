// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER	0x91B9
#endif

#include "OpenGL3.h"


struct FMacOpenGL : public FOpenGL3
{
private:
	static void MacGetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint *OutResult);
	static void MacQueryTimestampCounter(GLuint QueryID);
	static void MacBeginQuery(GLenum QueryType, GLuint QueryId);
	static void MacEndQuery(GLenum QueryType);
	static void MacGetQueryObject(GLuint QueryId, EQueryMode QueryMode, uint64 *OutResult);
	
	/** Must we employ a workaround for radr://15553950, TTP# 315197 */
	static bool MustFlushTexStorage(void);
	
	/** Is the current renderer the Intel HD3000? */
	static bool IsIntelHD3000();
	
public:
	static void ProcessQueryGLInt();
	static void ProcessExtensions(const FString& ExtensionsString);
	static uint64 GetVideoMemorySize();
	
	static FORCEINLINE ERHIFeatureLevel::Type GetFeatureLevel()
	{
		// The Intel HD 3000 on OS X can't cope with our deferred renderer, but ES2 mode works fine :(
		static bool bForceFeatureLevelES2 = FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES2"));
		if (bForceFeatureLevelES2 || IsIntelHD3000())
		{
			return ERHIFeatureLevel::ES2;
		}
		
		return FOpenGL3::GetFeatureLevel();
	}
	
	static FORCEINLINE EShaderPlatform GetShaderPlatform()
	{
		// The Intel HD 3000 on OS X can't cope with our deferred renderer, but ES2 mode works fine :(
		static bool bForceFeatureLevelES2 = FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES2"));
		if (bForceFeatureLevelES2 || IsIntelHD3000())
		{
			return SP_OPENGL_PCES2;
		}
		
		// Shader platform
		switch(GetMajorVersion())
		{
			case 3:
				return SP_OPENGL_SM4_MAC;
			case 4:
				return GetMinorVersion() > 2 ? SP_OPENGL_SM5 : SP_OPENGL_SM4_MAC;
			default:
				return SP_OPENGL_SM4_MAC;
		}
	}
	
	static FORCEINLINE bool SupportsSeparateAlphaBlend()				{ return bSupportsDrawBuffersBlend; }
	static FORCEINLINE bool SupportsSeamlessCubeMap()					{ return true; }
	static FORCEINLINE bool SupportsClientStorage()						{ return !bUsingApitrace; }
	static FORCEINLINE bool SupportsTextureRange()						{ return !bUsingApitrace; }
	static FORCEINLINE bool SupportsDepthBoundsTest()					{ return true; }
	static FORCEINLINE bool SupportsDrawIndirect()						{ return bSupportsDrawIndirect; }
	
	static FORCEINLINE void Flush()
	{
		glFlushRenderAPPLE();
	}
	
	// Workaround Mac-specific MapBuffer issues without exposing the changes to other platforms
	static FORCEINLINE void* MapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize, EResourceLockMode LockMode)
	{
		GLenum Access;
		switch ( LockMode )
		{
			case RLM_ReadOnly:
				Access = GL_MAP_READ_BIT;
				break;
			case RLM_WriteOnly:
				Access = (GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_WRITE_BIT);
				break;
			case RLM_WriteOnlyUnsynchronized:
				Access = (GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
				break;
			case RLM_WriteOnlyPersistent:
				Access = (GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
				break;
			case RLM_ReadWrite:
			default:
				Access = (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
		}
		return glMapBufferRange(Type, InOffset, InSize, Access);
	}
	
	static void DeleteTextures(GLsizei Number, const GLuint* Textures);
	
	static void BufferSubData(GLenum Target, GLintptr Offset, GLsizeiptr Size, const GLvoid* Data);
	
	static FORCEINLINE void ClearBufferfi(GLenum Buffer, GLint DrawBufferIndex, GLfloat Depth, GLint Stencil)
	{
		if(FPlatformMisc::IsRunningOnMavericks() || !GSupportsDepthFetchDuringDepthTest)
		{
			switch (Buffer)
			{
				case GL_DEPTH_STENCIL:	// Clear depth and stencil separately to avoid an AMD Dx00 bug which causes depth to clear, but not stencil.
					// Especially irritatingly this bug will not manifest when stepping though the program with GL Profiler.
					// This was a bug found by me during dev. on Tropico 3 circa. Q4 2011 in the ATi Mac 2xx0 & 4xx0 drivers.
					// It was never fixed & has re-emerged in the AMD Mac FirePro Dx00 drivers.
					// Also, on NVIDIA depth must be cleared first.
				case GL_DEPTH:	// Clear depth only
					ClearBufferfv(GL_DEPTH, 0, &Depth);
					
					// If not also clearing depth break
					if(Buffer == GL_DEPTH)
					{
						break;
					}
					// Otherwise fall through to perform a separate stencil clear.
				case GL_STENCIL:	// Clear stencil only
					ClearBufferiv(GL_STENCIL, 0, (const GLint*)&Stencil);
					break;
					
				default:
					break;	// impossible anyway
			}
		}
		else
		{
			glClearBufferfi(Buffer, DrawBufferIndex, Depth, Stencil);
		}
	}

	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint *OutResult)
	{
		MacGetQueryObject(QueryId, QueryMode, OutResult);
	}
	static FORCEINLINE void QueryTimestampCounter(GLuint QueryID)
	{
		MacQueryTimestampCounter(QueryID);
	}
	static FORCEINLINE void BeginQuery(GLenum QueryType, GLuint QueryId)
	{
		MacBeginQuery(QueryType, QueryId);
	}
	static FORCEINLINE void EndQuery(GLenum QueryType)
	{
		MacEndQuery(QueryType);
	}
	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, uint64 *OutResult)
	{
		MacGetQueryObject(QueryId, QueryMode, OutResult);
	}

	static FORCEINLINE void LabelObject(GLenum Type, GLuint Object, const ANSICHAR* Name)
	{
		if(glLabelObjectEXT)
		{
			glLabelObjectEXT(Type, Object, 0, Name);
		}
	}

	static FORCEINLINE void PushGroupMarker(const ANSICHAR* Name)
	{
		if(glPushGroupMarkerEXT)
		{
			glPushGroupMarkerEXT(0, Name);
		}
	}

	static FORCEINLINE void PopGroupMarker()
	{
		if(glPopGroupMarkerEXT)
		{
			glPopGroupMarkerEXT();
		}
	}
	
	static FORCEINLINE void TexStorage3D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLsizei Depth, GLenum Format, GLenum Type)
	{
		if( glTexStorage3D )
		{
			glTexStorage3D( Target, Levels, InternalFormat, Width, Height, Depth);
			if(MustFlushTexStorage())
			{
				glFlushRenderAPPLE(); // Workaround for radr://15553950, TTP# 315197
			}
		}
		else
		{
			const bool bArrayTexture = Target == GL_TEXTURE_2D_ARRAY || (bSupportsTextureCubeMapArray && Target == GL_TEXTURE_CUBE_MAP_ARRAY);
			
			for(uint32 MipIndex = 0; MipIndex < uint32(Levels); MipIndex++)
			{
				glTexImage3D(
							 Target,
							 MipIndex,
							 InternalFormat,
							 FMath::Max<uint32>(1,(Width >> MipIndex)),
							 FMath::Max<uint32>(1,(Height >> MipIndex)),
							 (bArrayTexture) ? Depth : FMath::Max<uint32>(1,(Depth >> MipIndex)),
							 0,
							 Format,
							 Type,
							 NULL
							 );
			}
		}
	}
	
	static FORCEINLINE bool TexStorage2D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLenum Format, GLenum Type, uint32 Flags)
	{
		if( glTexStorage2D )
		{
			glTexStorage2D(Target, Levels, InternalFormat, Width, Height);
			if(MustFlushTexStorage())
			{
				glFlushRenderAPPLE(); // Workaround for radr://15553950, TTP# 315197
			}
			return true;
		}
		else
		{
			return false;
		}
	}
	
	static FORCEINLINE void TextureRange(GLenum Target, GLsizei Length, const GLvoid *Pointer)
	{
		glTextureRangeAPPLE(Target, Length, Pointer);
	}
	
	static FORCEINLINE void BlendFuncSeparatei(GLuint Buf, GLenum SrcRGB, GLenum DstRGB, GLenum SrcAlpha, GLenum DstAlpha)
	{
		if(glBlendFuncSeparatei)
		{
			glBlendFuncSeparatei(Buf, SrcRGB, DstRGB, SrcAlpha, DstAlpha);
		}
		else
		{
			UE_LOG(LogRHI, Fatal, TEXT("OpenGL state on draw requires setting different blend operation or factors to different render targets. This is not supported on Mac OS X GL 3 contexts!"));
		}
	}
	
	static FORCEINLINE void BlendEquationSeparatei(GLuint Buf, GLenum ModeRGB, GLenum ModeAlpha)
	{
		if(glBlendEquationSeparatei)
		{
			glBlendEquationSeparatei(Buf, ModeRGB, ModeAlpha);
		}
		else
		{
			UE_LOG(LogRHI, Fatal, TEXT("OpenGL state on draw requires setting different blend operation or factors to different render targets. This is not supported on Mac OS X GL 3 contexts!"));
		}
	}
	
	static FORCEINLINE void BlendFunci(GLuint Buf, GLenum Src, GLenum Dst)
	{
		if(glBlendFunci)
		{
			glBlendFunci(Buf, Src, Dst);
		}
		else
		{
			UE_LOG(LogRHI, Fatal, TEXT("OpenGL state on draw requires setting different blend operation or factors to different render targets. This is not supported on Mac OS X GL 3 contexts!"));
		}
	}
	
	static FORCEINLINE void BlendEquationi(GLuint Buf, GLenum Mode)
	{
		if(glBlendEquationi)
		{
			glBlendEquationi(Buf, Mode);
		}
		else
		{
			UE_LOG(LogRHI, Fatal, TEXT("OpenGL state on draw requires setting different blend operation or factors to different render targets. This is not supported on Mac OS X GL 3 contexts!"));
		}
	}
	
	static FORCEINLINE void PatchParameteri(GLenum Pname, GLint Value)
	{
		if(glPatchParameteri)
		{
			glPatchParameteri(Pname, Value);
		}
		else
		{
			UE_LOG(LogRHI, Fatal, TEXT("Tessellation not supported on Mac OS X GL 3 contexts!"));
		}
	}
	
	static FORCEINLINE void DepthBounds(GLfloat Min, GLfloat Max)
	{
		glDepthBoundsEXT( Min, Max);
	}
	
	static FORCEINLINE void DrawArraysIndirect (GLenum Mode, const void *Offset)
	{
		if(glDrawArraysIndirect)
		{
			glDrawArraysIndirect( Mode, Offset);
		}
		else
		{
			UE_LOG(LogRHI, Fatal, TEXT("Draw Indirect not supported on Mac OS X GL 3 contexts!"));
		}
	}
	
	static FORCEINLINE void DrawElementsIndirect (GLenum Mode, GLenum Type, const void *Offset)
	{
		if(glDrawElementsIndirect)
		{
			glDrawElementsIndirect( Mode, Type, Offset);
		}
		else
		{
			UE_LOG(LogRHI, Fatal, TEXT("Draw Indirect not supported on Mac OS X GL 3 contexts!"));
		}
	}
	
private:
	/** Detects apitrace interception library & equivalents */
	static bool bUsingApitrace;
	/** GL_ARB_texture_cube_map_array */
	static bool bSupportsTextureCubeMapArray;
	/** GL_ARB_texture_storage */
	static PFNGLTEXSTORAGE2DPROC glTexStorage2D;
	static PFNGLTEXSTORAGE3DPROC glTexStorage3D;
	/** GL_ARB_draw_buffers_blend */
	static bool bSupportsDrawBuffersBlend;
	static PFNGLBLENDFUNCSEPARATEIARBPROC glBlendFuncSeparatei;
	static PFNGLBLENDEQUATIONSEPARATEIARBPROC glBlendEquationSeparatei;
	static PFNGLBLENDFUNCIARBPROC glBlendFunci;
	static PFNGLBLENDEQUATIONIARBPROC glBlendEquationi;
	/** GL_EXT_debug_label */
	static PFNGLLABELOBJECTEXTPROC glLabelObjectEXT;
	/** GL_EXT_debug_marker */
	static PFNGLPUSHGROUPMARKEREXTPROC glPushGroupMarkerEXT;
	static PFNGLPOPGROUPMARKEREXTPROC glPopGroupMarkerEXT;
	/** GL_ARB_tessellation_shader */
	static PFNGLPATCHPARAMETERIPROC glPatchParameteri;
	/** GL_ARB_draw_indirect */
	static bool bSupportsDrawIndirect;
	static PFNGLDRAWARRAYSINDIRECTPROC glDrawArraysIndirect;
	static PFNGLDRAWELEMENTSINDIRECTPROC glDrawElementsIndirect;
};


typedef FMacOpenGL FOpenGL;
