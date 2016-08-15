// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Object.h"
#include "UnrealString.h"
#include "../Public/AudioDevice.h"
#include "UTAudioSettings.generated.h"

UENUM(BlueprintType)
namespace EUTSoundClass
{
	enum Type
	{
		Master UMETA(DisplayName = "Master"),
		Music UMETA(DisplayName = "Music"),
		SFX UMETA(DisplayName = "SFX"),
		Voice UMETA(DisplayName = "Voice"),
		VOIP UMETA(DisplayName ="VOIP"),
		Music_Stingers UMETA(DisplayName="Stingers"),
		GameMusic UMETA(DisplayName="Game Music"),
		// should always be last, used to size arrays
		MAX UMETA(Hidden)
	};
}

UCLASS()
class UNREALTOURNAMENT_API UUTAudioSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual void SetSoundClassVolume(EUTSoundClass::Type SoundClass, float NewVolume);
protected:
	virtual void SetSoundClassVolumeByName(const FString& SoundClassName, float NewVolume);
};
