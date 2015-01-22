// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "InterpTrackInstSound.generated.h"

UCLASS()
class UInterpTrackInstSound : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	float LastUpdatePosition;

	UPROPERTY(transient)
	class UAudioComponent* PlayAudioComp;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	virtual void TermTrackInst(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

