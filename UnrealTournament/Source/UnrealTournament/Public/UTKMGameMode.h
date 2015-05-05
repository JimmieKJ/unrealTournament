// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTKMGameMode.generated.h"

UCLASS(Config=Game)
class UNREALTOURNAMENT_API AUTKMGameMode : public AUTGameMode
{
	GENERATED_UCLASS_BODY()

	void ScoreKill(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType);

	UPROPERTY(Config)
	FString CrownClassName;

	virtual bool ValidateHat(AUTPlayerState* HatOwner, const FString& HatClass);

	void BecomeKing(AUTPlayerState* KingPlayerState);
	void KingHasBeenKilled(AUTPlayerState* KingPlayerState, APawn* KingPawn, AUTPlayerState* KillerPlayerState);
};



