// Copyright 1998 - 2015 Epic Games, Inc.All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunScoring.h"
#include "UTFlagRunGameState.h"
#include "StatNames.h"

AUTFlagRunScoring::AUTFlagRunScoring(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AUTFlagRunScoring::BeginPlay()
{
	Super::BeginPlay();

	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTFlagRunScoring::FlagHeldTimer, 1.0f, true);
}

void AUTFlagRunScoring::FlagHeldTimer()
{
}

void AUTFlagRunScoring::ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* ScoringPawn, AUTPlayerState* ScorerPS, FName Reason, float TimeLimit, int32 FlagCapScore)
{
	AUTFlagRunGameState* CTFGameState = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (!CTFGameState)
	{
		return;
	}
	if (Reason == FName("FlagCapture"))
	{
		FCTFScoringPlay NewScoringPlay;
		NewScoringPlay.Team = ScorerPS->Team;
		NewScoringPlay.ScoredBy = FSafePlayerName(ScorerPS);
		NewScoringPlay.TeamScores[0] = CTFGameState->Teams[0] ? CTFGameState->Teams[0]->Score : 0;
		NewScoringPlay.TeamScores[1] = CTFGameState->Teams[1] ? CTFGameState->Teams[1]->Score : 0;
		NewScoringPlay.TeamScores[ScorerPS->Team->TeamIndex] += FlagCapScore;
		NewScoringPlay.RemainingTime = CTFGameState->bPlayingAdvantage ? 0.f : CTFGameState->GetClockTime();
		NewScoringPlay.bAnnihilation = false;
		NewScoringPlay.bDefenseWon = false;
		NewScoringPlay.Period = CTFGameState->CTFRound;

		ScorerPS->FlagCaptures++;
		ScorerPS->ModifyStatsValue(NAME_FlagCaptures, 1);
		NewScoringPlay.ScoredByCaps = ScorerPS->FlagCaptures;

		NewScoringPlay.RedBonus = CTFGameState->Teams[0] ? CTFGameState->Teams[0]->RoundBonus : 0;
		NewScoringPlay.BlueBonus = CTFGameState->Teams[1] ? CTFGameState->Teams[1]->RoundBonus : 0;
		CTFGameState->AddScoringPlay(NewScoringPlay);
	}
}

/** Save partial credit for flag carrier damage. */
void AUTFlagRunScoring::ScoreDamage(int32 DamageAmount, AUTPlayerState* VictimPS, AUTPlayerState* AttackerPS)
{
}

void AUTFlagRunScoring::ScoreKill(AController* Killer, AController* Victim, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
}