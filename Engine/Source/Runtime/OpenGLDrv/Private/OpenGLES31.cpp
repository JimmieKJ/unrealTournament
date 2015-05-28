// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGL3.cpp: OpenGL 3.2 implementation.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

#if OPENGL_ES31

static TAutoConsoleVariable<int32> CVarDisjointTimerQueries(
	TEXT("r.DisjointTimerQueries"),
	1,
	TEXT("If set to 1, allows GPU time to be measured (e.g. STAT UNIT). It defaults to 0 because some devices supports it but very slowly."),
	ECVF_RenderThreadSafe);

GLsizei FOpenGLES31::NextTextureName = OPENGL_NAME_CACHE_SIZE;
GLuint FOpenGLES31::TextureNamesCache[OPENGL_NAME_CACHE_SIZE];
GLsizei FOpenGLES31::NextBufferName= OPENGL_NAME_CACHE_SIZE;
GLuint FOpenGLES31::BufferNamesCache[OPENGL_NAME_CACHE_SIZE];

GLint FOpenGLES31::MaxComputeTextureImageUnits = -1;
GLint FOpenGLES31::MaxComputeUniformComponents = -1;

GLint FOpenGLES31::TimestampQueryBits = 0;
bool FOpenGLES31::bDebugContext = false;

bool FOpenGLES31::bSupportsTessellation = false;
bool FOpenGLES31::bSupportsTextureView = false;
bool FOpenGLES31::bSupportsSeparateAlphaBlend = false;

bool FOpenGLES31::bES2Fallback = true;

/** GL_OES_vertex_array_object */
bool FOpenGLES31::bSupportsVertexArrayObjects = false;

/** GL_OES_mapbuffer */
bool FOpenGLES31::bSupportsMapBuffer = false;

/** GL_OES_depth_texture */
bool FOpenGLES31::bSupportsDepthTexture = false;

/** GL_ARB_occlusion_query2, GL_EXT_occlusion_query_boolean */
bool FOpenGLES31::bSupportsOcclusionQueries = false;

/** GL_OES_rgb8_rgba8 */
bool FOpenGLES31::bSupportsRGBA8 = false;

/** GL_APPLE_texture_format_BGRA8888 */
bool FOpenGLES31::bSupportsBGRA8888 = false;

/** GL_EXT_discard_framebuffer */
bool FOpenGLES31::bSupportsDiscardFrameBuffer = false;

/** GL_OES_vertex_half_float */
bool FOpenGLES31::bSupportsVertexHalfFloat = false;

/** GL_OES_texture_float */
bool FOpenGLES31::bSupportsTextureFloat = false;

/** GL_OES_texture_half_float */
bool FOpenGLES31::bSupportsTextureHalfFloat = false;

/** GL_EXT_color_buffer_half_float */
bool FOpenGLES31::bSupportsColorBufferHalfFloat = false;

/** GL_EXT_shader_framebuffer_fetch */
bool FOpenGLES31::bSupportsShaderFramebufferFetch = false;

/** GL_EXT_sRGB */
bool FOpenGLES31::bSupportsSGRB = false;

/** GL_NV_texture_compression_s3tc, GL_EXT_texture_compression_s3tc */
bool FOpenGLES31::bSupportsDXT = false;

/** GL_IMG_texture_compression_pvrtc */
bool FOpenGLES31::bSupportsPVRTC = false;

/** GL_ATI_texture_compression_atitc, GL_AMD_compressed_ATC_texture */
bool FOpenGLES31::bSupportsATITC = false;

/** GL_OES_compressed_ETC1_RGB8_texture */
bool FOpenGLES31::bSupportsETC1 = false;

/** OpenGL ES 3.0 profile */
bool FOpenGLES31::bSupportsETC2 = false;

/** GL_FRAGMENT_SHADER, GL_LOW_FLOAT */
int FOpenGLES31::ShaderLowPrecision = 0;

/** GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT */
int FOpenGLES31::ShaderMediumPrecision = 0;

/** GL_FRAGMENT_SHADER, GL_HIGH_FLOAT */
int FOpenGLES31::ShaderHighPrecision = 0;

/** GL_NV_framebuffer_blit */
bool FOpenGLES31::bSupportsNVFrameBufferBlit = false;

/** GL_OES_packed_depth_stencil */
bool FOpenGLES31::bSupportsPackedDepthStencil = false;

/** textureCubeLodEXT */
bool FOpenGLES31::bSupportsTextureCubeLodEXT = true;

/** GL_EXT_shader_texture_lod */
bool FOpenGLES31::bSupportsShaderTextureLod = false;

/** textureCubeLod */
bool FOpenGLES31::bSupportsShaderTextureCubeLod = true;

/** GL_APPLE_copy_texture_levels */
bool FOpenGLES31::bSupportsCopyTextureLevels = false;

/** GL_EXT_texture_storage */
bool FOpenGLES31::bSupportsTextureStorageEXT = false;

/* This is a hack to remove the calls to "precision sampler" defaults which are produced by the cross compiler however don't compile on some android platforms */
bool FOpenGLES31::bRequiresDontEmitPrecisionForTextureSamplers = false;

/* Some android platforms require textureCubeLod to be used some require textureCubeLodEXT however they either inconsistently or don't use the GL_TextureCubeLodEXT extension definition */
bool FOpenGLES31::bRequiresTextureCubeLodEXTToTextureCubeLodDefine = false;

/* This is a hack to remove the gl_FragCoord if shader will fail to link if exceeding the max varying on android platforms */
bool FOpenGLES31::bRequiresGLFragCoordVaryingLimitHack = false;

/* This hack fixes an issue with SGX540 compiler which can get upset with some operations that mix highp and mediump */
bool FOpenGLES31::bRequiresTexture2DPrecisionHack = false;

/** GL_EXT_disjoint_timer_query or GL_NV_timer_query*/
bool FOpenGLES31::bSupportsDisjointTimeQueries = false;

/** Some timer query implementations are never disjoint */
bool FOpenGLES31::bTimerQueryCanBeDisjoint = true;

/** GL_NV_timer_query for timestamp queries */
bool FOpenGLES31::bSupportsNvTimerQuery = false;

GLint FOpenGLES31::MajorVersion = 0;
GLint FOpenGLES31::MinorVersion = 0;

bool FOpenGLES31::SupportsAdvancedFeatures()
{
	bool bResult = true;
	GLint MajorVersion = 0;
	GLint MinorVersion = 0;

	const FString ExtensionsString = ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_EXTENSIONS));

	if (FString(ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VERSION))).Contains(TEXT("OpenGL ES 3.")))
	{
		glGetIntegerv(GL_MAJOR_VERSION, &MajorVersion);
		glGetIntegerv(GL_MINOR_VERSION, &MinorVersion);

		// Check for minimum ES 3.1 + extensions support to avoid the ES2 fallback
		bResult = (MajorVersion == 3 && MinorVersion >= 1);

		bResult &= ExtensionsString.Contains(TEXT("GL_ANDROID_extension_pack_es31a"));
		bResult &= ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float"));
	}
	else
	{
		bResult = false;
	}

	return bResult;
}

bool FOpenGLES31::SupportsDisjointTimeQueries()
{
	bool bAllowDisjointTimerQueries = false;
	bAllowDisjointTimerQueries = (CVarDisjointTimerQueries.GetValueOnRenderThread() == 1);
	return bSupportsDisjointTimeQueries && bAllowDisjointTimerQueries;
}

void FOpenGLES31::ProcessQueryGLInt()
{
	if(bES2Fallback)
	{
		LOG_AND_GET_GL_INT(GL_MAX_VARYING_VECTORS, 0, MaxVaryingVectors);
		LOG_AND_GET_GL_INT(GL_MAX_VERTEX_UNIFORM_VECTORS, 0, MaxVertexUniformComponents);
		LOG_AND_GET_GL_INT(GL_MAX_FRAGMENT_UNIFORM_VECTORS, 0, MaxPixelUniformComponents);
		MaxVaryingVectors *= 4;
		MaxVertexUniformComponents *= 4;
		MaxPixelUniformComponents *= 4;
		MaxGeometryUniformComponents = 0;

		MaxGeometryTextureImageUnits = 0;
		MaxHullTextureImageUnits = 0;
		MaxDomainTextureImageUnits = 0;
	}
	else
	{
		GET_GL_INT(GL_MAX_VARYING_VECTORS, 0, MaxVaryingVectors);
		GET_GL_INT(GL_MAX_VERTEX_UNIFORM_COMPONENTS, 0, MaxVertexUniformComponents);
		GET_GL_INT(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, 0, MaxPixelUniformComponents);
		GET_GL_INT(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT, 0, MaxGeometryUniformComponents);

		GET_GL_INT(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT, 0, MaxGeometryTextureImageUnits);

		GET_GL_INT(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, 0, MaxComputeTextureImageUnits);
		GET_GL_INT(GL_MAX_COMPUTE_UNIFORM_COMPONENTS, 0, MaxComputeUniformComponents);

		if (bSupportsTessellation)
		{
			GET_GL_INT(GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS_EXT, 0, MaxHullUniformComponents);
			GET_GL_INT(GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS_EXT, 0, MaxDomainUniformComponents);
			GET_GL_INT(GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS_EXT, 0, MaxHullTextureImageUnits);
			GET_GL_INT(GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS_EXT, 0, MaxDomainTextureImageUnits);
		}
	}
	
	// No timestamps
	//LOG_AND_GET_GL_QUERY_INT(GL_TIMESTAMP, 0, TimestampQueryBits);
}

void FOpenGLES31::ProcessExtensions( const FString& ExtensionsString )
{
	// Version setup first, need to check string for 3 or higher, then can use integer queries
	if (SupportsAdvancedFeatures())
	{
		glGetIntegerv(GL_MAJOR_VERSION, &MajorVersion);
		glGetIntegerv(GL_MINOR_VERSION, &MinorVersion);

		bES2Fallback = false;
		UE_LOG(LogRHI, Log, TEXT("bES2Fallback = false"));
	}
	else
	{
		MajorVersion = 2;
		MinorVersion = 0;
		bES2Fallback = true;
		UE_LOG(LogRHI, Log, TEXT("bES2Fallback = true"));
	}

	bSupportsSeparateAlphaBlend = ExtensionsString.Contains(TEXT("GL_EXT_draw_buffers_indexed"));

	static auto CVarAllowHighQualityLightMaps = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HighQualityLightMaps"));

	if (!bES2Fallback)
	{
		// only supported if we are at a minimum bar
		bSupportsTessellation = ExtensionsString.Contains(TEXT("GL_EXT_tessellation_shader"));
		bSupportsTextureView = ExtensionsString.Contains(TEXT("GL_EXT_texture_view"));
		CVarAllowHighQualityLightMaps->Set(1);
	}
	else
	{
		CVarAllowHighQualityLightMaps->Set(0);
	}

	ProcessQueryGLInt();
	FOpenGLBase::ProcessExtensions(ExtensionsString);

	bSupportsMapBuffer = ExtensionsString.Contains(TEXT("GL_OES_mapbuffer"));
	bSupportsDepthTexture = ExtensionsString.Contains(TEXT("GL_OES_depth_texture"));
	bSupportsOcclusionQueries = ExtensionsString.Contains(TEXT("GL_ARB_occlusion_query2")) || ExtensionsString.Contains(TEXT("GL_EXT_occlusion_query_boolean"));
	bSupportsRGBA8 = ExtensionsString.Contains(TEXT("GL_OES_rgb8_rgba8"));
	bSupportsBGRA8888 = ExtensionsString.Contains(TEXT("GL_APPLE_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_IMG_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_format_BGRA8888"));
	bSupportsVertexHalfFloat = ExtensionsString.Contains(TEXT("GL_OES_vertex_half_float"));
	bSupportsTextureFloat = ExtensionsString.Contains(TEXT("GL_OES_texture_float"));
	bSupportsTextureHalfFloat = ExtensionsString.Contains(TEXT("GL_OES_texture_half_float"));
	bSupportsSGRB = ExtensionsString.Contains(TEXT("GL_EXT_sRGB"));
	bSupportsColorBufferHalfFloat = ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float"));
	bSupportsShaderFramebufferFetch = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_NV_shader_framebuffer_fetch"));
	// @todo es3: SRGB support does not work with our texture format setup (ES2 docs indicate that internalFormat and format must match, but they don't at all with sRGB enabled)
	//             One possible solution us to use GLFormat.InternalFormat[bSRGB] instead of GLFormat.Format
	bSupportsSGRB = false;//ExtensionsString.Contains(TEXT("GL_EXT_sRGB"));
	bSupportsDXT = ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc"));
	bSupportsPVRTC = ExtensionsString.Contains(TEXT("GL_IMG_texture_compression_pvrtc")) ;
	bSupportsATITC = ExtensionsString.Contains(TEXT("GL_ATI_texture_compression_atitc")) || ExtensionsString.Contains(TEXT("GL_AMD_compressed_ATC_texture"));
	bSupportsETC1 = ExtensionsString.Contains(TEXT("GL_OES_compressed_ETC1_RGB8_texture"));
	bSupportsVertexArrayObjects = ExtensionsString.Contains(TEXT("GL_OES_vertex_array_object")) ;
	bSupportsDiscardFrameBuffer = ExtensionsString.Contains(TEXT("GL_EXT_discard_framebuffer"));
	bSupportsNVFrameBufferBlit = ExtensionsString.Contains(TEXT("GL_NV_framebuffer_blit"));
	bSupportsPackedDepthStencil = ExtensionsString.Contains(TEXT("GL_OES_packed_depth_stencil"));
	bSupportsShaderTextureLod = ExtensionsString.Contains(TEXT("GL_EXT_shader_texture_lod"));
	bSupportsTextureStorageEXT = ExtensionsString.Contains(TEXT("GL_EXT_texture_storage"));
	bSupportsCopyTextureLevels = bSupportsTextureStorageEXT && ExtensionsString.Contains(TEXT("GL_APPLE_copy_texture_levels"));
	bSupportsDisjointTimeQueries = ExtensionsString.Contains(TEXT("GL_EXT_disjoint_timer_query"));// || ExtensionsString.Contains(TEXT("GL_NV_timer_query"));
	bTimerQueryCanBeDisjoint = !ExtensionsString.Contains(TEXT("GL_NV_timer_query"));
	bSupportsNvTimerQuery = ExtensionsString.Contains(TEXT("GL_NV_timer_query"));

	// Report shader precision
	int Range[2];
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_FLOAT, Range, &ShaderLowPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, Range, &ShaderMediumPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, Range, &ShaderHighPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader lowp precision: %d"), ShaderLowPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader mediump precision: %d"), ShaderMediumPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader highp precision: %d"), ShaderHighPrecision);
	
	// Test whether the GPU can support volume-texture rendering.
	// There is no API to query this - you just have to test whether a 3D texture is framebuffer-complete.
	if (!bES2Fallback)
	{
		GLuint FrameBuffer;
		glGenFramebuffers(1, &FrameBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FrameBuffer);
		GLuint VolumeTexture;
		glGenTextures(1, &VolumeTexture);
		glBindTexture(GL_TEXTURE_3D, VolumeTexture);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 256, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glFramebufferTextureEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, VolumeTexture, 0);

		bSupportsVolumeTextureRendering = (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		glDeleteTextures(1, &VolumeTexture);
		glDeleteFramebuffers(1, &FrameBuffer);
	}

	bSupportsCopyImage = ExtensionsString.Contains(TEXT("GL_EXT_copy_image"));
}

#endif
