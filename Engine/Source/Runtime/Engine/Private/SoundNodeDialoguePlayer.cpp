// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ActiveSound.h"
#include "Sound/DialogueWave.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundNodeDialoguePlayer.h"

USoundNodeDialoguePlayer::USoundNodeDialoguePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USoundNodeDialoguePlayer::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	USoundBase* SoundBase = DialogueWaveParameter.DialogueWave ? DialogueWaveParameter.DialogueWave->GetWaveFromContext(DialogueWaveParameter.Context) : NULL;
	if (SoundBase)
	{
		if (bLooping)
		{
			FSoundParseParameters UpdatedParams = ParseParams;
			UpdatedParams.bLooping = true;
			SoundBase->Parse(AudioDevice, NodeWaveInstanceHash, ActiveSound, UpdatedParams, WaveInstances);
		}
		else
		{
			SoundBase->Parse(AudioDevice, NodeWaveInstanceHash, ActiveSound, ParseParams, WaveInstances);
		}
	}
}

float USoundNodeDialoguePlayer::GetDuration()
{
	USoundBase* SoundBase = DialogueWaveParameter.DialogueWave ? DialogueWaveParameter.DialogueWave->GetWaveFromContext(DialogueWaveParameter.Context) : NULL;
	float Duration = 0.f;
	if (SoundBase)
	{
		if (bLooping)
		{
			Duration = INDEFINITELY_LOOPING_DURATION;
		}
		else
		{
			Duration = SoundBase->GetDuration();
		}
	}
	return Duration;
}

// A Wave Player is the end of the chain and has no children
int32 USoundNodeDialoguePlayer::GetMaxChildNodes() const
{
	return 0;
}
