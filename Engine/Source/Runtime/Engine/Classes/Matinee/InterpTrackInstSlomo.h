// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "InterpTrackInstSlomo.generated.h"

UCLASS()
class UInterpTrackInstSlomo : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()

	/** Backup of initial LevelInfo MatineeTimeDilation setting when interpolation started. */
	UPROPERTY()
	float OldTimeDilation;


	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	virtual void TermTrackInst(UInterpTrack* Track) override;
	virtual void SaveActorState(UInterpTrack* Track) override;
	virtual void RestoreActorState(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance

	/** @return whether the slomo track's effects should actually be applied. We want to only do this once for the server
	 * and not at all for the clients regardless of the number of instances created for the various players
	 * to avoid collisions and replication issues
	 */
	bool ShouldBeApplied();
};

