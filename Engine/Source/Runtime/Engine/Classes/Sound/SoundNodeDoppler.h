// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Sound/SoundNode.h"
#include "SoundNodeDoppler.generated.h"

struct FListener;

/** 
 * Computes doppler pitch shift
 */
UCLASS(hidecategories=Object, editinlinenew, meta=( DisplayName="Doppler" ))
class USoundNodeDoppler : public USoundNode
{
	GENERATED_UCLASS_BODY()

	/* How much to scale the doppler shift (1.0 is normal). */
	UPROPERTY(EditAnywhere, Category=Doppler )
	float DopplerIntensity;


public:
	// Begin USoundNode interface. 
	virtual void ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	// End USoundNode interface. 

protected:
	// @todo document
	float GetDopplerPitchMultiplier(FListener const& InListener, const FVector Location, const FVector Velocity) const;
};



