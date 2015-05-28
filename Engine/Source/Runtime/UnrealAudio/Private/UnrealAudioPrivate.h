// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "UnrealAudioModule.h"
#include "UnrealAudioDeviceModule.h"
#include "UnrealAudioTests.h"

#if ENABLE_UNREAL_AUDIO

DECLARE_LOG_CATEGORY_EXTERN(LogUnrealAudio, Log, All);

namespace UAudio
{
	class FUnrealAudioModule :	public IUnrealAudioModule
	{
	public:

		FUnrealAudioModule();
		~FUnrealAudioModule();

		bool Initialize() override;
		bool Initialize(const FString& DeviceModuleName) override;
		void Shutdown() override;

		class IUnrealAudioDeviceModule* GetDeviceModule();

		FName GetDefaultDeviceModuleName() const;

	private:

		bool InitializeInternal();
		static void InitializeTests(FUnrealAudioModule* Module);

		class IUnrealAudioDeviceModule* UnrealAudioDevice;
		FName ModuleName;
	};
}

#endif // #if ENABLE_UNREAL_AUDIO


