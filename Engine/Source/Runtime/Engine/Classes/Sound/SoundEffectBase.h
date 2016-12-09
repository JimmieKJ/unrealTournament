// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "SoundEffectBase.generated.h"

class USoundEffectPreset;

UCLASS()
class ENGINE_API USoundEffectBase : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	virtual ~USoundEffectBase();

	/** Returns if the submix is active or bypassing audio. */
	bool IsActive() const { return bIsActive; }

	/** Disables the submix effect. */
	void Disable() { bIsActive = false; }

	/** Enables the submix effect. */
	void Enable() { bIsActive = true; }

	/** Sets the sound effect's preset, overriding any properties to the presets properties. */
	void SetPreset(USoundEffectPreset* InPreset);

protected:

	/** Return the class of the effect. */
	virtual UClass* GetEffectClass() const 
	{
		return nullptr;
	};

	USoundEffectPreset* SoundEffectPreset;
	uint32 PresetSettingsSize;

	TArray<uint8> RawPresetDataScratchInputBuffer;
	TArray<uint8> RawPresetDataScratchOutputBuffer;
	TQueue<TArray<uint8>> EffectSettingsQueue;
	TArray<uint8> CurrentAudioThreadSettingsData;

	FThreadSafeBool bIsRunning;
	FThreadSafeBool bIsActive;
};

