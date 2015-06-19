// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ModuleInterface.h"

// Only enable unreal audio on windows or mac
#if PLATFORM_WINDOWS || PLATFORM_MAC
#define ENABLE_UNREAL_AUDIO 1
#else
#define ENABLE_UNREAL_AUDIO 0
#endif

namespace UAudio
{
	class UNREALAUDIO_API IUnrealAudioModule :  public IModuleInterface
	{
	public:
		virtual ~IUnrealAudioModule() {}

		virtual bool Initialize()
		{
			return false;
		}

		virtual bool Initialize(const FString& DeviceModuleName)
		{
			return false;
		}

		virtual void Shutdown()
		{
		}
	};
}



