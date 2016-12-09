// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundEffectSubmix.h"

USoundEffectSubmixPreset::USoundEffectSubmixPreset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


USoundEffectSubmix::USoundEffectSubmix(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USoundEffectSubmix::ProcessAudio(FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	bIsRunning = true;

	// Update the latest version of the effect settings on the audio thread. If there are new settings,
	// then RawPresetDataScratchOutputBuffer will have the last data.
	EffectSettingsQueue.Dequeue(RawPresetDataScratchOutputBuffer);

	InData.PresetData = RawPresetDataScratchOutputBuffer.GetData();

	// Only process the effect if the effect is active
	if (bIsActive)
	{
		OnProcessAudio(InData, OutData);
	}
	else
	{
		// otherwise, bypass the effect and move inputs to outputs
		OutData.AudioBuffer = MoveTemp(InData.AudioBuffer);
	}
}
