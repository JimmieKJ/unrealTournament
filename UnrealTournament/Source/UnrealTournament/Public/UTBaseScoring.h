// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
/** Handles individual scoring in CTF matches. */

#pragma once
#include "UTBaseScoring.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTBaseScoring : public AInfo
{
	GENERATED_UCLASS_BODY()

	virtual void InitFor(class AUTGameMode* Game);

	/** Called when game object scores (i.e. flag cap). */
	virtual void ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* ScoringPawn, AUTPlayerState* ScorerPS, FName Reason, int32 FlagCapScore);

	/** Called when game object scores (i.e. flag cap). */
	virtual void ScoreObjective(AUTGameObjective* GameObjective, AUTCharacter* ScoringPawn, AUTPlayerState* ScorerPS, FName Reason, int32 ObjectiveScore);

	/** Called when player causes damage. */
	virtual void ScoreDamage(int32 DamageAmount, AUTPlayerState* VictimPS, AUTPlayerState* AttackerPS);

	/** Called when player kills someone. */
	virtual void ScoreKill(AController* Killer, AController* Victim, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType);
};
