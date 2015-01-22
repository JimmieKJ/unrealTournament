// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Matinee/InterpTrackInst.h"
#include "InterpTrackInstMove.generated.h"

UCLASS(MinimalAPI)
class UInterpTrackInstMove : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()

	/** Saved position. Used in editor for resetting when quitting Matinee. */
	UPROPERTY()
	FVector ResetLocation;

	/** Saved rotation. Used in editor for resetting when quitting Matinee. */
	UPROPERTY()
	FRotator ResetRotation;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

