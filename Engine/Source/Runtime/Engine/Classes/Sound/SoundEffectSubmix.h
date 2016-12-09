// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Sound/SoundEffectPreset.h"
#include "Sound/SoundEffectBase.h"
#include "SoundEffectSubmix.generated.h"

class USoundEffectSubmix;

/** Derived class for source effects. */
UCLASS(config = Engine, hidecategories = Object, abstract, editinlinenew, BlueprintType)
class ENGINE_API USoundEffectSubmixPreset : public USoundEffectPreset
{
	GENERATED_UCLASS_BODY()

	/** Create a new submix effect instance. */
	virtual USoundEffectSubmix* CreateNewEffect() const { return nullptr; }
};

/** Struct which has data needed to initialize the submix effect. */
struct FSoundEffectSubmixInitData
{
	void* PresetSettings;
	float SampleRate;
	int32 NumOutputChannels;
};

/** Struct which supplies audio data to submix effects on game thread. */
struct FSoundEffectSubmixInputData
{
	void* PresetData;
	TArray<float>* AudioBuffer;
	int32 NumChannels;
	double AudioClock;
};

struct FSoundEffectSubmixOutputData
{
	TArray<float>* AudioBuffer;
};

UCLASS(config = Engine, hidecategories = Object, abstract, editinlinenew, BlueprintType)
class ENGINE_API USoundEffectSubmix : public USoundEffectBase
{
	GENERATED_UCLASS_BODY()

	/** Called on an audio effect at initialization on main thread before audio processing begins. */
	virtual void Init(const FSoundEffectSubmixInitData& InSampleRate) PURE_VIRTUAL(USoundEffectSubmix::Init, ;);

	/** Called on game thread to allow submix effect to query game data if needed. */
	virtual void Tick() {}

	/** Process the input block of audio. Called on audio thread. */
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) PURE_VIRTUAL(USoundEffectSubmix::OnProcessAudio, ;);

	/** Processes audio in the source effect. */
	void ProcessAudio(FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData);

protected:
	UClass* GetEffectClass() const { return GetClass(); }

};
