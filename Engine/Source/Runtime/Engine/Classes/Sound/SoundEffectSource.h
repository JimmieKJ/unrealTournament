// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sound/SoundEffectPreset.h"
#include "Sound/SoundEffectBase.h"

#include "SoundEffectSource.generated.h"

class USoundEffectSource;

/** Derived class for source effects. */
UCLASS(config = Engine, abstract, editinlinenew, BlueprintType)
class ENGINE_API USoundEffectSourcePreset : public USoundEffectPreset
{
	GENERATED_UCLASS_BODY()

	/** Create a new source sound effect instance. */
	virtual USoundEffectSource* CreateNewEffect() const { return nullptr; }
};

/** Struct which has data needed to initialize the source effect. */
struct FSoundEffectSourceInitData
{
	void* PresetSettings;
	float SampleRate;
	int32 NumSourceChannels;
	float SourceDuration;
	double AudioClock;
	FVector SourcePosition;
	FVector ListenerPosition;
};

/** Struct which has data to initialize the source effect. */
struct FSoundEffectSourceInputData
{
	void* PresetData;
	TArray<float> AudioBuffer;
	int32 NumSourceChannels;
	FVector SourcePosition;
	FVector LeftChannelPosition;
	FVector RightChannelPosition;
	FVector ListenerPosition;
	float CurrentVolume;
	float CurrentPitch;
	float CurrentPlayTime;
	float Duration;
	float Distance;
	double AudioClock;
	int32 CurrentLoopCount;
	int32 MaxLoopCount;
	uint8 bLooping : 1;
	uint8 bIsSpatialized : 1;
};

struct FSoundEffectSourceOutputData
{
	TArray<float> AudioBuffer;
	int32 OutputChannels;
	uint8 bIsSpatialized:1;
};

UCLASS(config = Engine, hidecategories = Object, abstract, editinlinenew, BlueprintType)
class ENGINE_API USoundEffectSource : public USoundEffectBase
{
	GENERATED_UCLASS_BODY()

		/** Called on an audio effect at initialization on main thread before audio processing begins. */
		virtual void Init(const FSoundEffectSourceInitData& InSampleRate) PURE_VIRTUAL(USoundEffectSource::Init, ;);

	/** Process the input block of audio. Called on audio thread. */
	virtual void OnProcessAudio(const FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData) PURE_VIRTUAL(USoundEffectSource::OnProcessAudio, ;);

private:
	/** Processes audio in the source effect. */
	void ProcessAudio(FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData);

protected:
	UClass* GetEffectClass() const override { return GetClass(); };

	// Allow FAudioMixerSubmix to call ProcessAudio
	friend class FMixerSubmix;
};

