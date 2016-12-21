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

void AUTBaseScoring::ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* ScoringPawn, AUTPlayerState* ScorerPS, FName Reason, float TimeLimit, int32 FlagCapScore)
{
}

/** Save partial credit for flag carrier damage. */
void AUTBaseScoring::ScoreDamage(int32 DamageAmount, AUTPlayerState* VictimPS, AUTPlayerState* AttackerPS)
{
}

void AUTBaseScoring::ScoreKill(AController* Killer, AController* Victim, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
}