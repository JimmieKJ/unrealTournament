// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidOpenGLPrivate.h: Code shared betweeen AndroidOpenGL and AndroidGL4OpenGL
=============================================================================*/
#pragma once

#include "AndroidApplication.h"

bool GAndroidGPUInfoReady = false;

class FAndroidGPUInfo
{
public:
	static FAndroidGPUInfo& Get()
	{
		static FAndroidGPUInfo This;
		return This;
	}

	FString GPUFamily;
	FString GLVersion;
	bool bSupportsFloatingPointRenderTargets;
	bool bSupportsFrameBufferFetch;
	TArray<FString> TargetPlatformNames;

private:
	FAndroidGPUInfo()
	{
#if PLATFORM_ANDROIDGL4
		// hard coded for the time being
		GPUFamily = "NVIDIA Tegra";
		GLVersion = "4.4.0";
		bSupportsFloatingPointRenderTargets = true;
		TargetPlatformNames.Add(TEXT("Android_GL4"));
#else
		// this is only valid in the game thread, make sure we are initialized there before being called on other threads!
		check(IsInGameThread())

		// make sure GL is started so we can get the supported formats
		AndroidEGL* EGL = AndroidEGL::GetInstance();

		if (!EGL->IsInitialized())
		{
			FAndroidAppEntry::PlatformInit();
#if PLATFORM_ANDROIDES31
			EGL->InitSurface(true);
#endif
		}
#if !PLATFORM_ANDROIDES31
		EGL->InitSurface(true);
#endif
		EGL->SetCurrentSharedContext();

		// get extensions
		// Process the extension caps directly here, as FOpenGL might not yet be setup
		// Do not process extensions here, because extension pointers may not be setup
		const ANSICHAR* GlGetStringOutput = (const ANSICHAR*) glGetString(GL_EXTENSIONS);
		FString ExtensionsString = GlGetStringOutput;

		GPUFamily = (const ANSICHAR*)glGetString(GL_RENDERER);
		check(!GPUFamily.IsEmpty());

		GLVersion = (const ANSICHAR*)glGetString(GL_VERSION);

		const bool bES30Support = GLVersion.Contains(TEXT("OpenGL ES 3."));

#if PLATFORM_ANDROIDES31
		TargetPlatformNames.Add(TEXT("Android_ES31"));
#else
		// highest priority is the per-texture version
		if (ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr")))
		{
			TargetPlatformNames.Add(TEXT("Android_ASTC"));
		}
		if (ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc")))
		{
			TargetPlatformNames.Add(TEXT("Android_DXT"));
		}
		if (ExtensionsString.Contains(TEXT("GL_ATI_texture_compression_atitc")) || ExtensionsString.Contains(TEXT("GL_AMD_compressed_ATC_texture")))
		{
			TargetPlatformNames.Add(TEXT("Android_ATC"));
		}
		if (ExtensionsString.Contains(TEXT("GL_IMG_texture_compression_pvrtc")))
		{
			TargetPlatformNames.Add(TEXT("Android_PVRTC"));
		}
		if (bES30Support)
		{
			TargetPlatformNames.Add(TEXT("Android_ETC2"));
		}
		if (ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr")))
		{
			TargetPlatformNames.Add(TEXT("Android_ASTC"));
		}

		// all devices support ETC
		TargetPlatformNames.Add(TEXT("Android_ETC1"));

		// finally, generic Android
		TargetPlatformNames.Add(TEXT("Android"));

#endif

		bSupportsFloatingPointRenderTargets = ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float"));
		bSupportsFrameBufferFetch = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_NV_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_ARM_shader_framebuffer_fetch"));
#endif
		GAndroidGPUInfoReady = true;
	}
};
