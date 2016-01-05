// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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

AUTCTFRoundGame::AUTCTFRoundGame(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	GoalScore = 3;
	TimeLimit = 0;
	DisplayName = NSLOCTEXT("UTGameMode", "CTFR", "Capture - Round based CTF");

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
	Super::HandleExitingIntermission();
	CTFGameState->CTFRound++;
}

void AUTCTFRoundGame::InitGameState()
{
	Super::InitGameState();
	CTFGameState->CTFRound = 1;
}
