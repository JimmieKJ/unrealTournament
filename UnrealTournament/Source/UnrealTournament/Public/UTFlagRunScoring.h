// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
/** Handles individual scoring in CTF matches. */

#pragma once
#include "UTBaseScoring.h"
#include "UTResetInterface.h"
#include "UTFlagRunScoring.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTFlagRunScoring : public AUTBaseScoring
{
	GENERATED_UCLASS_BODY()

	virtual void BeginPlay() override;

	virtual void FlagHeldTimer();

	virtual void ScoreDamage(int32 DamageAmount, AUTPlayerState* Victim, AUTPlayerState* Attacker) override;
	virtual void ScoreKill(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;
	virtual void ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason, int32 FlagCapScore = 1) override;
	virtual void ScoreObjective(AUTGameObjective* GameObjective, AUTCharacter* ScoringPawn, AUTPlayerState* ScorerPS, FName Reason, int32 ObjectiveScore) override;
};
