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
	AUTFlagRunGameState* CTFGameState = GetWorld()->GetGameState<AUTFlagRunGameState>();
	AUTCarriedObject* Flag = CTFGameState ? CTFGameState->GetOffenseFlag() : nullptr;
	if (Flag && Flag->Holder)
	{
		Flag->Holder->ModifyStatsValue(NAME_FlagHeldTime, 1.f);
	}
}

void AUTFlagRunScoring::ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* ScoringPawn, AUTPlayerState* ScorerPS, FName Reason, int32 FlagCapScore)
{
	AUTFlagRunGameState* CTFGameState = GetWorld()->GetGameState<AUTFlagRunGameState>();
	if (CTFGameState && (Reason == FName("FlagCapture")))
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

void AUTFlagRunScoring::ScoreObjective(AUTGameObjective* GameObjective, AUTCharacter* ScoringPawn, AUTPlayerState* ScorerPS, FName Reason, int32 ObjectiveScore)
{

}

void AUTFlagRunScoring::ScoreDamage(int32 DamageAmount, AUTPlayerState* VictimPS, AUTPlayerState* AttackerPS)
{
	if (!VictimPS || !VictimPS->CarriedObject || (DamageAmount <= 0))
	{
		return;
	}
	if (AttackerPS && (AttackerPS != VictimPS))
	{
		AttackerPS->LastShotFCTime = GetWorld()->GetTimeSeconds();
		AttackerPS->LastShotFC = VictimPS;
		AttackerPS->ModifyStatsValue(NAME_EnemyFCDamage, DamageAmount);
	}
}

void AUTFlagRunScoring::ScoreKill(AController* Killer, AController* Victim, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	AUTFlagRunGameState* CTFGameState = GetWorld()->GetGameState<AUTFlagRunGameState>();
	AUTPlayerState* VictimPS = Victim ? Cast<AUTPlayerState>(Victim->PlayerState) : NULL;
	AUTPlayerState* KillerPS = Killer ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
	if (VictimPS && KillerPS && (VictimPS != KillerPS) && CTFGameState && !CTFGameState->OnSameTeam(Killer, Victim))
	{
		if (KillerPS->Team)
		{
			KillerPS->Team->ModifyStatsValue(NAME_TeamKills, 1);
		}
		if (VictimPS->CarriedObject)
		{
			KillerPS->LastKilledFCTime = GetWorld()->GetTimeSeconds();
			// bonus based on flag hold time
			KillerPS->ModifyStatsValue(NAME_FCKills, 1);
		}
	}
	if (VictimPS)
	{
		VictimPS->LastShotFCTime = 0.f;
		VictimPS->LastShotFC = NULL;
	}
}

