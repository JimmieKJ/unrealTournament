// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
// Includes for audio dsp helper classes
#include "AudioMixerSubmixEffectEQ.generated.h"

// ========================================================================
// FAudiomixerSourceEffectFilterSettings
// ========================================================================

USTRUCT()
struct FSubmixEffectEQSettings
{
	GENERATED_USTRUCT_BODY()

	// TODO
};

// ========================================================================
// USoundEffectSubmixEQ
// Class which processes audio streams and uses parameters defined in the preset class.
// ========================================================================

UCLASS()
class AUDIOMIXER_API USoundEffectSubmixEQ : public USoundEffectSource
{
	GENERATED_BODY()
	
	// Called on an audio effect at initialization on main thread before audio processing begins.
	void Init(const FSoundEffectSourceInitData& InSampleRate) override;

	// Process the input block of audio. Called on audio thread.
	void OnProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) override;
};

// ========================================================================
// UMyTestSourceEffectPreset
// This code exposes your preset settings and effect class to the editor.
// Do not modify this code!
// ========================================================================

UCLASS()
class AUDIOMIXER_API USoundEffectSubmixEQPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()
	
	void* GetSettings() override { return (void*)&Settings; }
	uint32 GetSettingsSize() const override { return sizeof(Settings); }
	UScriptStruct* GetSettingsStruct() const override { return FSubmixEffectEQSettings::StaticStruct(); }
	UClass* GetEffectClass() const override { return USoundEffectSubmixEQ::StaticClass(); }
	FText GetAssetActionName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundEffectSubmixEQPreset", "SoundEffectSubmixEQPreset"); }

	// FSubmixEffectEQSettings Preset Settings
	UPROPERTY(EditAnywhere, Category = SoundSourceEffect)
	FSubmixEffectEQSettings Settings;
};
