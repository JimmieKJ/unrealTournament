// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTSCTFGame.h"
#include "UTGauntletGame.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTGauntletGame : public AUTSCTFGame
{
	GENERATED_UCLASS_BODY()

	void GenericPlayerInitialization(AController* C);
	void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType);

	virtual bool CanBoost(AUTPlayerController* Who);
	virtual bool AttemptBoost(AUTPlayerController* Who);

};