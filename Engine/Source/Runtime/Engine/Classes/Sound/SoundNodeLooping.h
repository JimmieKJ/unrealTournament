// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

 
#pragma once
#include "Sound/SoundNode.h"
#include "SoundNodeLooping.generated.h"

/** 
 * Defines how a sound loops; either indefinitely, or for a set number of times
 */
UCLASS(hidecategories=Object, editinlinenew, MinimalAPI, meta=( DisplayName="Looping" ))
class USoundNodeLooping : public USoundNode
{
	GENERATED_UCLASS_BODY()

	/* The amount of times to loop */
	UPROPERTY(EditAnywhere, Category = Looping, meta = (ClampMin = 1))
	int32 LoopCount;

	/* If enabled, the node will continue to loop indefinitely regardless of the Loop Count value. */
	UPROPERTY(EditAnywhere, Category = Looping)
	uint32 bLoopIndefinitely : 1;

public:	
	// Begin USoundNode interface. 
	virtual bool NotifyWaveInstanceFinished( struct FWaveInstance* WaveInstance ) override;
	virtual float MaxAudibleDistance( float CurrentMaxDistance ) override 
	{ 
		return( WORLD_MAX ); 
	}
	virtual float GetDuration( void ) override;
	virtual void ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	// End USoundNode interface. 
};



