// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGauntletGame.h"
#include "UTGauntletGameState.h"
#include "UTGauntletFlag.h"
#include "UTGauntletGameMessage.h"
#include "UTCountDownMessage.h"
#include "Net/UnrealNetwork.h"

AUTGauntletGameState::AUTGauntletGameState(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	AttackingTeam = 255;
}

bool AUTGauntletGameState::CanShowBoostMenu(AUTPlayerController* Target)
{
	return Super::CanShowBoostMenu(Target) || Target->GetPawn() == nullptr;
}

FText AUTGauntletGameState::GetGameStatusText(bool bForScoreboard)
{
	if (HasMatchEnded())
	{
		return GameOverStatus;
	}
	else if (GetMatchState() == MatchState::MapVoteHappening)
	{
		return MapVoteStatus;
	}
	else if (CTFRound > 0)
	{
		return FText::Format(NSLOCTEXT("UTGauntletGameState","GoalScoreMsg","First to {0} caps"), FText::AsNumber(GoalScore));
	}
	else if (IsMatchIntermission())
	{
		return IntermissionStatus;
	}

	return AUTGameState::GetGameStatusText(bForScoreboard);
}
