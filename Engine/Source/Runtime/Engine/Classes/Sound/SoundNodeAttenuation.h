// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

 
#pragma once
#include "Sound/SoundNode.h"
#include "SoundAttenuation.h"
#include "SoundNodeAttenuation.generated.h"

struct FAttenuationSettings;
class USoundAttenuation;

/** 
 * Defines how a sound's volume changes based on distance to the listener
 */
UCLASS(hidecategories=Object, editinlinenew, MinimalAPI, meta=( DisplayName="Attenuation" ))
class USoundNodeAttenuation : public USoundNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Attenuation, meta=(EditCondition="!bOverrideAttenuation"))
	USoundAttenuation* AttenuationSettings;

	UPROPERTY(EditAnywhere, Category=Attenuation, meta=(EditCondition="bOverrideAttenuation"))
	FAttenuationSettings AttenuationOverrides;

	UPROPERTY(EditAnywhere, Category=Attenuation)
	uint32 bOverrideAttenuation:1;

public:
	//~ Begin USoundNode Interface. 
	virtual void ParseNodes( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	virtual float MaxAudibleDistance( float CurrentMaxDistance ) override;
	//~ End USoundNode Interface. 

	ENGINE_API FAttenuationSettings* GetAttenuationSettingsToApply();
};



