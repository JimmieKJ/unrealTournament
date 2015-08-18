// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CEF3UtilsPrivatePCH.h"

DEFINE_LOG_CATEGORY(LogCEF3Utils);

IMPLEMENT_MODULE(FDefaultModuleImpl, CEF3Utils);

#if WITH_CEF3
namespace CEF3Utils
{
#if PLATFORM_WINDOWS
    void* CEF3DLLHandle = nullptr;
	void* D3DHandle = nullptr;
	void* MPEGHandle = nullptr;
	void* GLESHandle = nullptr;
    void* EGLHandle = nullptr;
#endif

	void LoadCEF3Modules()
	{
#if PLATFORM_WINDOWS
	#if PLATFORM_64BITS
		FString DllPath(FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries/ThirdParty/CEF3/Win64")));
	#else
		FString DllPath(FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries/ThirdParty/CEF3/Win32")));
	#endif

		CEF3DLLHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine(*DllPath, TEXT("libcef.dll")));
		if (CEF3DLLHandle == NULL)
		{
			int32 ErrorNum = FPlatformMisc::GetLastError();
			TCHAR ErrorMsg[1024];
			FPlatformMisc::GetSystemErrorMessage(ErrorMsg, 1024, ErrorNum);
			UE_LOG(LogCEF3Utils, Error, TEXT("Failed to get CEF3 DLL handle: %s (%d)"), ErrorMsg, ErrorNum);
		}

	#if WINVER >= 0x600 // Different dll used pre-Vista
		D3DHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine(*DllPath, TEXT("d3dcompiler_47.dll")));
	#else
		D3DHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine(*DllPath, TEXT("d3dcompiler_43.dll")));
	#endif
		MPEGHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine(*DllPath, TEXT("ffmpegsumo.dll")));
		GLESHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine(*DllPath, TEXT("libGLESv2.dll")));
		EGLHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine(*DllPath, TEXT("libEGL.dll")));
#endif
	}

	void UnloadCEF3Modules()
	{
#if PLATFORM_WINDOWS
		FPlatformProcess::FreeDllHandle(CEF3DLLHandle);
		CEF3DLLHandle = nullptr;
		FPlatformProcess::FreeDllHandle(D3DHandle);
		D3DHandle = nullptr;
		FPlatformProcess::FreeDllHandle(MPEGHandle);
		MPEGHandle = nullptr;
		FPlatformProcess::FreeDllHandle(GLESHandle);
		GLESHandle = nullptr;
		FPlatformProcess::FreeDllHandle(EGLHandle);
		EGLHandle = nullptr;
#endif
	}
};
#endif //WITH_CEF3
