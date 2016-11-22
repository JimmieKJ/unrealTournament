// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFRoundGame.h"
#include "UTCTFRoundGameState.h"
#include "UTCTFGameMode.h"
#include "UTPowerupSelectorUserWidget.h"
#include "Net/UnrealNetwork.h"
#include "UTCTFScoring.h"
#include "StatNames.h"
#include "UTCountDownMessage.h"
#include "UTAnnouncer.h"

AUTCTFRoundGameState::AUTCTFRoundGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RemainingPickupDelay = 0;
	TiebreakValue = 0;
}

void AUTCTFRoundGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTCTFRoundGameState, IntermissionTime);
	DOREPLIFETIME(AUTCTFRoundGameState, OffenseKills);
	DOREPLIFETIME(AUTCTFRoundGameState, DefenseKills);
	DOREPLIFETIME(AUTCTFRoundGameState, RemainingPickupDelay);
	DOREPLIFETIME(AUTCTFRoundGameState, TiebreakValue);
	DOREPLIFETIME(AUTCTFRoundGameState, RedLivesRemaining);
	DOREPLIFETIME(AUTCTFRoundGameState, BlueLivesRemaining);
}

void AUTCTFRoundGameState::DefaultTimer()
{
	Super::DefaultTimer();
	if (bIsAtIntermission)
	{
		IntermissionTime--;
	}
	else if ((GetNetMode() != NM_DedicatedServer) && IsMatchInProgress())
	{
		UpdateTimeMessage();
	}
}

void AUTCTFRoundGameState::UpdateTimeMessage()
{
}

float AUTCTFRoundGameState::GetIntermissionTime()
{
	return IntermissionTime;
}

FText AUTCTFRoundGameState::GetRoundStatusText(bool bForScoreboard)
{
	FFormatNamedArguments Args;
	Args.Add("RoundNum", FText::AsNumber(CTFRound));
	Args.Add("NumRounds", FText::AsNumber(NumRounds));
	return (NumRounds > 0) ? FText::Format(FullRoundInProgressStatus, Args) : FText::Format(RoundInProgressStatus, Args);
}

FText AUTCTFRoundGameState::GetGameStatusText(bool bForScoreboard)
{
	if (HasMatchEnded())
	{
		return GameOverStatus;
	}
	else if (GetMatchState() == MatchState::MapVoteHappening)
	{
		return MapVoteStatus;
	}
	else if ((CTFRound > 0) &&  IsMatchInProgress())
	{
		return GetRoundStatusText(bForScoreboard);
	}
	else if (IsMatchIntermission())
	{
		return IntermissionStatus;
	}

	return AUTGameState::GetGameStatusText(bForScoreboard);
}

/** Returns true if P1 should be sorted before P2.  */
bool AUTCTFRoundGameState::InOrder(AUTPlayerState* P1, AUTPlayerState* P2)
{
	// spectators are sorted last
	if (P1->bOnlySpectator)
	{
		return P2->bOnlySpectator;
	}
	else if (P2->bOnlySpectator)
	{
		return true;
	}

	// sort by Score
	if (P1->Score < P2->Score)
	{
		return false;
	}
	if (P1->Score == P2->Score)
	{
		// if score tied, use round kills to sort
		if (P1->RoundKills > P2->RoundKills)
			return false;

		// keep local player highest on list
		if ((P1->RoundKills == P2->RoundKills) && (Cast<APlayerController>(P2->GetOwner()) != NULL))
		{
			ULocalPlayer* LP2 = Cast<ULocalPlayer>(Cast<APlayerController>(P2->GetOwner())->Player);
			if (LP2 != NULL)
			{
				// make sure ordering is consistent for splitscreen players
				ULocalPlayer* LP1 = Cast<ULocalPlayer>(Cast<APlayerController>(P2->GetOwner())->Player);
				return (LP1 != NULL);
			}
		}
	}
	return true;
}

bool AUTCTFRoundGameState::IsTeamOnOffense(int32 TeamNumber) const
{
	return false;
}

bool AUTCTFRoundGameState::IsTeamOnDefense(int32 TeamNumber) const
{
	return false;
}

 
