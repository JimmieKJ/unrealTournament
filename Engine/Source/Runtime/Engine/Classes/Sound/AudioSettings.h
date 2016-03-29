// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioSettings.generated.h"


USTRUCT()
struct ENGINE_API FAudioQualitySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Quality")
	FText DisplayName;

	// The number of audio channels that can be used at once
	// NOTE: Some platforms may cap this value to a lower setting regardless of what the settings request
	UPROPERTY(EditAnywhere, Category="Quality", meta=(ClampMin="1"))
	int32 MaxChannels;

	FAudioQualitySettings()
		: MaxChannels(32)
	{
	}
};

/**
 * Audio settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Audio"))
class ENGINE_API UAudioSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeChainProperty( struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	/** The SoundClass assigned to newly created sounds */
	UPROPERTY(config, EditAnywhere, Category="Audio", meta=(AllowedClasses="SoundClass", DisplayName="Default Sound Class"))
	FStringAssetReference DefaultSoundClassName;

	/** The SoundConcurrency assigned to newly created sounds */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (AllowedClasses = "SoundConcurrency", DisplayName = "Default Sound Concurrency"))
	FStringAssetReference DefaultSoundConcurrencyName;

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

	UPROPERTY(config, EditAnywhere, Category="Quality")
	TArray<FAudioQualitySettings> QualityLevels;

	const FAudioQualitySettings& GetQualityLevelSettings(int32 QualityLevel) const;

private:

#if WITH_EDITOR
	TArray<FAudioQualitySettings> CachedQualityLevels;
#endif

	void AddDefaultSettings();
};
