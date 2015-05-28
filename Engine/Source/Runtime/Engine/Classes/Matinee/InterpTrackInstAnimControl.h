// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "InterpTrackInstAnimControl.generated.h"

UCLASS(MinimalAPI)
class UInterpTrackInstAnimControl : public UInterpTrackInst
{
	GENERATED_UCLASS_BODY()

	/**
	 */
	UPROPERTY(transient)
	float LastUpdatePosition;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	FVector InitPosition;

	UPROPERTY(transient)
	FRotator InitRotation;

#endif // WITH_EDITORONLY_DATA

	// Begin UInterpTrackInst Instance
	virtual void InitTrackInst(UInterpTrack* Track) override;
	// End UInterpTrackInst Instance
};

