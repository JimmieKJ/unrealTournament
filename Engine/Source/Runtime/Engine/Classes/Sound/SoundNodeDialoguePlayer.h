// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Sound/SoundNode.h"
#include "SoundNodeDialoguePlayer.generated.h"

/**
 Sound node that contains a reference to the dialogue table to pull from and be played
*/
UCLASS(hidecategories = Object, editinlinenew, MinimalAPI, meta = (DisplayName = "Dialogue Player"))
class USoundNodeDialoguePlayer : public USoundNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DialoguePlayer)
	FDialogueWaveParameter DialogueWaveParameter;

	/* Whether the dialogue line should be played looping */
	UPROPERTY(EditAnywhere, Category=DialoguePlayer)
	uint32 bLooping:1;

public:	
	// Begin USoundNode Interface
	virtual int32 GetMaxChildNodes() const override;
	virtual float GetDuration() override;
	virtual void ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	// End USoundNode Interface

};

