// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioEntityManager.h"
#include "UnrealAudioModule.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	struct FSoundFileHandle : public FEntityHandle
	{
		FSoundFileHandle()
			: FEntityHandle(FEntityHandle::Create())
		{}

		FSoundFileHandle(const FEntityHandle& InEntityHandle)
			: FEntityHandle(InEntityHandle)
		{}
	};

	struct FVoiceHandle : public FEntityHandle
	{
		FVoiceHandle()
			: FEntityHandle(FEntityHandle::Create())
		{}

		FVoiceHandle(const FEntityHandle& InEntityHandle)
			: FEntityHandle(InEntityHandle)
		{}
	};

	struct FEmitterHandle : public FEntityHandle
	{
		FEmitterHandle()
			: FEntityHandle(FEntityHandle::Create())
		{}

		FEmitterHandle(const FEntityHandle& InEntityHandle)
			: FEntityHandle(InEntityHandle)
		{}
	};

}

#endif // #if ENABLE_UNREAL_AUDIO


