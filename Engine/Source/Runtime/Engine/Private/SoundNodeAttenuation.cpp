// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeAttenuation.h"

/*-----------------------------------------------------------------------------
	USoundNodeAttenuation implementation.
-----------------------------------------------------------------------------*/

USoundNodeAttenuation::USoundNodeAttenuation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

float USoundNodeAttenuation::MaxAudibleDistance( float CurrentMaxDistance ) 
{ 
	float RadiusMax = WORLD_MAX;
	
	if (bOverrideAttenuation)
	{
		RadiusMax = AttenuationOverrides.GetMaxDimension();
	}
	else if (AttenuationSettings)
	{
		RadiusMax = AttenuationSettings->Attenuation.GetMaxDimension();
	}

	return FMath::Max<float>( CurrentMaxDistance, RadiusMax );
}

FAttenuationSettings* USoundNodeAttenuation::GetAttenuationSettingsToApply()
{
	FAttenuationSettings* Settings = NULL;

	if (bOverrideAttenuation)
	{
		Settings = &AttenuationOverrides;
	}
	else if (AttenuationSettings)
	{
		Settings = &AttenuationSettings->Attenuation;
	}

	return Settings;
}

void USoundNodeAttenuation::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	FAttenuationSettings* Settings = (ActiveSound.bAllowSpatialization ? GetAttenuationSettingsToApply() : NULL);

	FSoundParseParameters UpdatedParseParams = ParseParams;
	if (Settings)
	{
		const FListener& Listener = AudioDevice->Listeners[0];
		Settings->ApplyAttenuation(UpdatedParseParams.Transform, Listener.Transform.GetTranslation(), UpdatedParseParams.Volume, UpdatedParseParams.HighFrequencyGain);
		UpdatedParseParams.OmniRadius = Settings->OmniRadius;
		UpdatedParseParams.bUseSpatialization |= Settings->bSpatialize;
	}
	else
	{
		UpdatedParseParams.bUseSpatialization = false;
	}

	Super::ParseNodes( AudioDevice, NodeWaveInstanceHash, ActiveSound, UpdatedParseParams, WaveInstances );
}