// Copyright 1998 - 2015 Epic Games, Inc.All Rights Reserved.
#include "UnrealTournament.h"
#include "UTBaseScoring.h"
#include "StatNames.h"

AUTBaseScoring::AUTBaseScoring(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AUTBaseScoring::InitFor(class AUTGameMode* Game)
{
}

void AUTBaseScoring::ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* ScoringPawn, AUTPlayerState* ScorerPS, FName Reason, int32 FlagCapScore)
{
}

void AUTBaseScoring::ScoreObjective(AUTGameObjective* GameObjective, AUTCharacter* ScoringPawn, AUTPlayerState* ScorerPS, FName Reason, int32 ObjectiveScore)
{}

/** Save partial credit for flag carrier damage. */
void AUTBaseScoring::ScoreDamage(int32 DamageAmount, AUTPlayerState* VictimPS, AUTPlayerState* AttackerPS)
{
}

void AUTBaseScoring::ScoreKill(AController* Killer, AController* Victim, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
}