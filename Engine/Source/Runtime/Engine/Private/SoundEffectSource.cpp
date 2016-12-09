// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundEffectSource.h"

USoundEffectSourcePreset::USoundEffectSourcePreset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

USoundEffectSource::USoundEffectSource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USoundEffectSource::ProcessAudio(FSoundEffectSourceInputData& InData, FSoundEffectSourceOutputData& OutData)
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
		OutData.bIsSpatialized = InData.bIsSpatialized;
		OutData.OutputChannels = InData.NumSourceChannels;
	}
}
