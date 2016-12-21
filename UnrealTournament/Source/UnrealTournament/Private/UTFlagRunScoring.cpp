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

/** Save partial credit for flag carrier damage. */
void AUTFlagRunScoring::ScoreDamage(int32 DamageAmount, AUTPlayerState* VictimPS, AUTPlayerState* AttackerPS)
{
}

void AUTFlagRunScoring::ScoreKill(AController* Killer, AController* Victim, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
}

void AUTFlagRunScoring::Reset_Implementation()
{

}
/*
round kills
round kill assists
round total kills
rallies (round and total)
rallies powered (round and total)
round FC kills
round damage caused
round flag held time
round denials
total denials

also per round multikills, sprees, etc

static const FName NAME_FlagCaptures(TEXT("FlagCaptures"));
static const FName NAME_FlagReturns(TEXT("FlagReturns"));
static const FName NAME_FlagAssists(TEXT("FlagAssists"));
static const FName NAME_FlagHeldDeny(TEXT("FlagHeldDeny"));
static const FName NAME_FlagHeldDenyTime(TEXT("FlagHeldDenyTime"));
static const FName NAME_FlagHeldTime(TEXT("FlagHeldTime"));
static const FName NAME_FlagReturnPoints(TEXT("FlagReturnPoints"));
static const FName NAME_CarryAssist(TEXT("CarryAssist"));
static const FName NAME_CarryAssistPoints(TEXT("CarryAssistPoints"));
static const FName NAME_FlagCapPoints(TEXT("FlagCapPoints"));
static const FName NAME_DefendAssist(TEXT("DefendAssist"));
static const FName NAME_DefendAssistPoints(TEXT("DefendAssistPoints"));
static const FName NAME_ReturnAssist(TEXT("ReturnAssist"));
static const FName NAME_ReturnAssistPoints(TEXT("ReturnAssistPoints"));
static const FName NAME_TeamCapPoints(TEXT("TeamCapPoints"));
static const FName NAME_EnemyFCDamage(TEXT("EnemyFCDamage"));
static const FName NAME_FCKills(TEXT("FCKills"));
static const FName NAME_FCKillPoints(TEXT("FCKillPoints"));
static const FName NAME_FlagSupportKills(TEXT("FlagSupportKills"));
static const FName NAME_FlagSupportKillPoints(TEXT("FlagSupportKillPoints"));
static const FName NAME_RegularKillPoints(TEXT("RegularKillPoints"));
static const FName NAME_FlagGrabs(TEXT("FlagGrabs"));
static const FName NAME_TeamFlagGrabs(TEXT("TeamFlagGrabs"));
static const FName NAME_TeamFlagHeldTime(TEXT("TeamFlagHeldTime"));
static const FName NAME_RalliesPowered(TEXT("RalliesPowered"));
static const FName NAME_Rallies(TEXT("Rallies"));
*/
