// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

 
#pragma once
#include "Sound/SoundNode.h"
#include "SoundNodeSoundClass.generated.h"

/** 
 * Remaps the SoundClass of SoundWaves underneath this
 */
UCLASS(hidecategories=Object, editinlinenew, MinimalAPI, meta=( DisplayName="SoundClass" ))
class USoundNodeSoundClass : public USoundNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=SoundClass)
	USoundClass* SoundClassOverride;

public:
	// Begin USoundNode interface. 
	virtual void ParseNodes( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	// End USoundNode interface. 
};
