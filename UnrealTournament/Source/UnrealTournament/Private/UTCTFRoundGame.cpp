// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTCTFRoundGame.h"
#include "UTCTFGameMessage.h"
#include "UTCTFRewardMessage.h"
#include "UTFirstBloodMessage.h"
#include "UTCountDownMessage.h"
#include "UTPickup.h"
#include "UTGameMessage.h"
#include "UTMutator.h"
#include "UTCTFSquadAI.h"
#include "UTWorldSettings.h"
#include "Widgets/SUTTabWidget.h"
#include "Dialogs/SUTPlayerInfoDialog.h"
#include "StatNames.h"
#include "Engine/DemoNetDriver.h"
#include "UTCTFScoreboard.h"
#include "UTShowdownGameMessage.h"

AUTCTFRoundGame::AUTCTFRoundGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	GoalScore = 3;
	TimeLimit = 0;
	DisplayName = NSLOCTEXT("UTGameMode", "CTFR", "Round based CTF");
	RoundLives = 15;
	bNeedFiveKillsMessage = true;
	FlagCapScore = 2;
}

void AUTCTFRoundGame::CreateGameURLOptions(TArray<TSharedPtr<TAttributePropertyBase>>& MenuProps)
{
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &GoalScore, TEXT("GoalScore"))));
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &BotFillCount, TEXT("BotFill"))));
	MenuProps.Add(MakeShareable(new TAttributePropertyBool(this, &bBalanceTeams, TEXT("BalanceTeams"))));
	MenuProps.Add(MakeShareable(new TAttributeProperty<int32>(this, &MercyScore, TEXT("MercyScore"))));
}

void AUTCTFRoundGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	TimeLimit = 0;
	if (GoalScore == 0)
	{
		GoalScore = 3;
	}
}

bool AUTCTFRoundGame::CheckScore_Implementation(AUTPlayerState* Scorer)
{
	if (Scorer->Team != NULL)
	{
		if (GoalScore > 0 && Scorer->Team->Score >= GoalScore)
		{
			EndGame(Scorer, FName(TEXT("scorelimit")));
		}
	}
	return true;
}

void AUTCTFRoundGame::HandleFlagCapture(AUTPlayerState* Holder)
{
	CheckScore(Holder);
	if (UTGameState->IsMatchInProgress())
	{
		SetMatchState(MatchState::MatchIntermission);
	}
}

int32 AUTCTFRoundGame::IntermissionTeamToView(AUTPlayerController* PC)
{
	if (LastTeamToScore)
	{
		return LastTeamToScore->TeamIndex;
	}
	return Super::IntermissionTeamToView(PC);
}

void AUTCTFRoundGame::BuildServerResponseRules(FString& OutRules)
{
	OutRules += FString::Printf(TEXT("Goal Score\t%i\t"), GoalScore);

	AUTMutator* Mut = BaseMutator;
	while (Mut)
	{
		OutRules += FString::Printf(TEXT("Mutator\t%s\t"), *Mut->DisplayName.ToString());
		Mut = Mut->NextMutator;
	}
}

void AUTCTFRoundGame::HandleExitingIntermission()
{
	InitRound();
	Super::HandleExitingIntermission();
}

void AUTCTFRoundGame::InitRound()
{
	bFirstBloodOccurred = false;
	bNeedFiveKillsMessage = true;
	if (CTFGameState)
	{
		CTFGameState->CTFRound++;
		CTFGameState->RedLivesRemaining = RoundLives;
		CTFGameState->BlueLivesRemaining = RoundLives;
		if (CTFGameState->FlagBases.Num() > 1)
		{
			CTFGameState->RedLivesRemaining += CTFGameState->FlagBases[0] ? CTFGameState->FlagBases[0]->RoundLivesAdjustment : 0;
			CTFGameState->BlueLivesRemaining += CTFGameState->FlagBases[1] ? CTFGameState->FlagBases[0]->RoundLivesAdjustment : 0;
		}
	}
}

void AUTCTFRoundGame::InitGameState()
{
	Super::InitGameState();
	InitRound();
	CTFGameState->CTFRound = 1;
}

void AUTCTFRoundGame::RestartPlayer(AController* aPlayer)
{
	Super::RestartPlayer(aPlayer);

	AUTPlayerState* PS = Cast<AUTPlayerState>(aPlayer->PlayerState);
	if (aPlayer->GetPawn() && (RoundLives > 0) && PS && PS->Team && CTFGameState && CTFGameState->IsMatchInProgress())
	{
		if (PS->Team->TeamIndex == 0)
		{
			CTFGameState->RedLivesRemaining--;
			if (CTFGameState->RedLivesRemaining <= 0)
			{
				CTFGameState->RedLivesRemaining = 0;
				ScoreOutOfLives(1);
				return;
			}
			else if (bNeedFiveKillsMessage && (CTFGameState->RedLivesRemaining == 5))
			{
				bNeedFiveKillsMessage = false;
				BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 7);
			}
		}
		else
		{
			CTFGameState->BlueLivesRemaining--;
			if (CTFGameState->BlueLivesRemaining <= 0)
			{
				CTFGameState->BlueLivesRemaining = 0;
				ScoreOutOfLives(0);
				return;
			}
			else if (bNeedFiveKillsMessage && (CTFGameState->BlueLivesRemaining == 5))
			{
				bNeedFiveKillsMessage = false;
				BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 7);
			}
		}
	}
}

void AUTCTFRoundGame::ScoreOutOfLives(int32 WinningTeamIndex)
{
	FindAndMarkHighScorer();
	AUTTeamInfo* WinningTeam = (Teams.Num() > WinningTeamIndex) ? Teams[WinningTeamIndex] : NULL;
	if (WinningTeam)
	{
		WinningTeam->Score++;
		WinningTeam->ForceNetUpdate();
		LastTeamToScore = WinningTeam;
		BroadcastLocalized(NULL, UUTShowdownGameMessage::StaticClass(), 3 + WinningTeam->TeamIndex);
		if (GoalScore > 0 && LastTeamToScore->Score >= GoalScore)
		{
			EndGame(NULL, FName(TEXT("scorelimit")));
		}
		else if (MercyScore > 0)
		{
			int32 Spread = LastTeamToScore->Score;
			for (AUTTeamInfo* OtherTeam : Teams)
			{
				if (OtherTeam != LastTeamToScore)
				{
					Spread = FMath::Min<int32>(Spread, LastTeamToScore->Score - OtherTeam->Score);
				}
			}
			if (Spread >= MercyScore)
			{
				EndGame(NULL, FName(TEXT("MercyScore")));
			}
		}
		if (UTGameState->IsMatchInProgress())
		{
			SetMatchState(MatchState::MatchIntermission);
		}
	}
}




