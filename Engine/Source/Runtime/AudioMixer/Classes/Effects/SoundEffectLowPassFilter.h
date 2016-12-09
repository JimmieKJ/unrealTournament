// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "SoundEffectLowPassFilter.generated.h"

// ========================================================================
// FSoundEffectLowPassFilterSettings
// ========================================================================

USTRUCT()
struct AUDIOMIXER_API FSoundEffectLowPassFilterSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = SoundEffect)
	float CutoffFrequency;

	UPROPERTY(EditAnywhere, Category = SoundEffect)
	float Q;
};

// ========================================================================
// USoundEffectLowPassFilter
// ========================================================================

UCLASS()
class AUDIOMIXER_API USoundEffectLowPassFilter : public USoundEffectSource
{
	GENERATED_BODY()

	// Called on an audio effect at initialization on main thread before audio processing begins.
	void Init(const FSoundEffectSourceInitData& InSampleRate) override {}
	
	// Process the input block of audio. Called on audio thread.
	void OnProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override {}

private:
	//Audio::FLowPass LowPassFilter;
};

// ========================================================================
// USoundEffectLowPassFilterPreset
// This code exposes your preset settings and effect class to the editor.
// Do not modify this code!
// ========================================================================

UCLASS()
class AUDIOMIXER_API USoundEffectLowPassFilterPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

	void* GetSettings() override { return (void*)&Settings; }
	uint32 GetSettingsSize() const override { return sizeof(Settings); }
	UScriptStruct* GetSettingsStruct() const override { return FSoundEffectLowPassFilterSettings::StaticStruct(); }
	UClass* GetEffectClass() const override { return USoundEffectLowPassFilter::StaticClass(); }

	UPROPERTY(EditAnywhere, Category = SoundEffect)
	FSoundEffectLowPassFilterSettings Settings;
};
