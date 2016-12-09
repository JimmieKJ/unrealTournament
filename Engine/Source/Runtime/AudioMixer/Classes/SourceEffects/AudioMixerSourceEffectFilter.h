// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
// Includes for audio dsp helper classes
#include "AudioMixerSourceEffectFilter.generated.h"

// ========================================================================
// FSourceEffectFilterSettings
// UStruct used to define user-exposed params for use with your effect.
// ========================================================================

UENUM()
namespace ESourceEffectFilter
{
	enum Type
	{
		LowPass,
		HighPass,
		BandPass,
		LowShelf,
		HighShelf
	};
}

USTRUCT()
struct FSourceEffectFilterSettings
{
	GENERATED_USTRUCT_BODY()

	// The type of filter to use for this source effect
	UPROPERTY(EditAnywhere, Category = SoundEffect)
	TEnumAsByte<enum ESourceEffectFilter::Type> FilterType;

	// The gain of the filter (only used in LowShelf and HighShelf types)
	UPROPERTY(EditAnywhere, Category = SoundEffect)
	float GainDb;

	// The corner or cutoff frequency of the filter in Hz
	UPROPERTY(EditAnywhere, Category = SoundEffect)
	float CutoffFrequency;

	// The Q of the filter. 
	UPROPERTY(EditAnywhere, Category = SoundEffect)
	float Q;
};

// ========================================================================
// USoundEffectSourceFilter
// Class which processes audio streams and uses parameters defined in the preset class.
// ========================================================================

UCLASS()
class AUDIOMIXER_API USourceEffectFilter : public USoundEffectSource
{
	GENERATED_BODY()
	
	// Called on an audio effect at initialization on main thread before audio processing begins.
	void Init(const FSoundEffectSourceInitData& InSampleRate) override;

	// Process the input block of audio. Called on audio thread.
	void OnProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;
};

// ========================================================================
// USoundEffectSourceFilterPreset
// This code exposes your preset settings and effect class to the editor.
// Do not modify this code!
// ========================================================================

UCLASS()
class AUDIOMIXER_API USoundEffectSourceFilterPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()
	
	void* GetSettings() override { return (void*)&Settings; }
	uint32 GetSettingsSize() const override { return sizeof(Settings); }
	UScriptStruct* GetSettingsStruct() const override { return FSourceEffectFilterSettings::StaticStruct(); }
	UClass* GetEffectClass() const override { return USourceEffectFilter::StaticClass(); }
	FText GetAssetActionName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundEffectSourceFilterPreset", "SoundEffectSourceFilterPreset"); }

	// Sound effect source filter settings.
	UPROPERTY(EditAnywhere, Category = SoundEffect)
	FSourceEffectFilterSettings Settings;
};
