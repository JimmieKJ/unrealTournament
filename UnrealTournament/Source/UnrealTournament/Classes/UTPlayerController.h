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
	virtual void ClientRestart_Implementation(APawn* NewPawn) OVERRIDE;
};



