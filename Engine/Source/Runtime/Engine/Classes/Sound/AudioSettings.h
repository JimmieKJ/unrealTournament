// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioSettings.generated.h"


/**
 * Audio settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Audio"))
class ENGINE_API UAudioSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	/** The SoundClass assigned to newly created sounds */
	UPROPERTY(config, EditAnywhere, Category="Audio", meta=(AllowedClasses="SoundClass", DisplayName="Default Sound Class"))
	FStringAssetReference DefaultSoundClassName;

	/** The SoundMix to use as base when no other system has specified a Base SoundMix */
	UPROPERTY(config, EditAnywhere, Category="Audio", meta=(AllowedClasses="SoundMix"))
	FStringAssetReference DefaultBaseSoundMix;
	
	/** Sound class to be used for the VOIP audio component */
	UPROPERTY(config, EditAnywhere, Category="Audio", meta=(AllowedClasses="SoundClass"))
	FStringAssetReference VoiPSoundClass;

	UPROPERTY(config, EditAnywhere, Category="Audio", AdvancedDisplay, meta=(ClampMin=0.1,ClampMax=1.5))
	float LowPassFilterResonance;

	/** How many streaming sounds can be played at the same time (if more are played they will be sorted by priority) */
	UPROPERTY(config, EditAnywhere, Category="Audio", meta=(ClampMin=0))
	int32 MaximumConcurrentStreams;
};
