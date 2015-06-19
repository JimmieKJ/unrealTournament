// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "SoundDefinitions.h"
#include "Sound/SoundNodeParamCrossFade.h"

/*-----------------------------------------------------------------------------
	USoundNodeParamCrossFade implementation.
-----------------------------------------------------------------------------*/
USoundNodeParamCrossFade::USoundNodeParamCrossFade(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

float USoundNodeParamCrossFade::GetCurrentDistance(FAudioDevice* AudioDevice, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams) const
{
	float ParamValue = 0.0f;
	
	ActiveSound.GetFloatParameter(ParamName, ParamValue);
	return ParamValue;
}

bool USoundNodeParamCrossFade::AllowCrossfading(FActiveSound& ActiveSound) const
{
	// Always allow parameter to control crossfading, even on 2D/preview sounds
	return true;
}

float USoundNodeParamCrossFade::MaxAudibleDistance(float CurrentMaxDistance)
{
	// Param-based crossfades are not a factor in max distance calculations
	return CurrentMaxDistance;
}
