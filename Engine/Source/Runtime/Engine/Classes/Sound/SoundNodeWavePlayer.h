// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Sound/SoundNode.h"
#include "SoundNodeWavePlayer.generated.h"

/** 
 * Sound node that contains a reference to the raw wave file to be played
 */
UCLASS(hidecategories=Object, editinlinenew, MinimalAPI, meta=( DisplayName="Wave Player" ))
class USoundNodeWavePlayer : public USoundNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=WavePlayer)
	USoundWave* SoundWave;

	UPROPERTY(EditAnywhere, Category=WavePlayer)
	uint32 bLooping:1;

public:	
	// Begin USoundNode Interface
	virtual int32 GetMaxChildNodes() const override;
	virtual float GetDuration() override;
	virtual void ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
#if WITH_EDITOR
	virtual FString GetTitle() const override;
#endif
	// End USoundNode Interface

};

