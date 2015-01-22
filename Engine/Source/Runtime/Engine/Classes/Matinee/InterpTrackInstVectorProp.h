// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "InterpTrackInstVectorProp.generated.h"

UCLASS()
class UInterpTrackInstVectorProp : public UInterpTrackInstProperty
{
	GENERATED_UCLASS_BODY()

	/** Pointer to FVector property in TrackObject. */
	FVector* VectorProp;

	/** Saved value for restoring state when exiting Matinee. */
	UPROPERTY()
	FVector ResetVector;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	virtual void SaveActorState(UInterpTrack* Track) override;
	virtual void RestoreActorState(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

