// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlayerController.generated.h"

UCLASS(dependson=UTCharacter, dependson=UTPlayerState, config=Game)
class AUTPlayerController : public APlayerController
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	AUTCharacter* UTCharacter;

	UPROPERTY()
	APlayerState* UTPlayerState;

	virtual void InitPlayerState();
	virtual void OnRep_PlayerState();
	virtual void SetPawn(APawn* InPawn);
	virtual void SetupInputComponent() OVERRIDE;

protected:

	/** weapon fire input handling -- NOTE: Just forward to the pawn */
	void OnFire();
	void OnStopFire();
	void OnAltFire();
	void OnStopAltFire();

	/** Handler for a touch input beginning. */
	void TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location);
};



