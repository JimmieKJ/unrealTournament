// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFRoundGameState.h"
#include "UTCTFGameMode.h"
#include "Net/UnrealNetwork.h"
#include "UTCTFScoring.h"
#include "StatNames.h"

AUTCTFRoundGameState::AUTCTFRoundGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GoldBonusText = NSLOCTEXT("FlagRun", "GoldBonusText", "GOLD");
	SilverBonusText = NSLOCTEXT("FlagRun", "SilverBonusText", "SILVER");
	BronzeBonusText = NSLOCTEXT("FlagRun", "BronzeBonusText", "BRONZE");
}

void AUTCTFRoundGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTCTFRoundGameState, IntermissionTime);
	DOREPLIFETIME(AUTCTFRoundGameState, OffenseKills);
	DOREPLIFETIME(AUTCTFRoundGameState, DefenseKills);
	DOREPLIFETIME(AUTCTFRoundGameState, OffenseKillsNeededForPowerup);
	DOREPLIFETIME(AUTCTFRoundGameState, DefenseKillsNeededForPowerup);
	DOREPLIFETIME(AUTCTFRoundGameState, bIsDefenseAbleToGainPowerup);
	DOREPLIFETIME(AUTCTFRoundGameState, bIsOffenseAbleToGainPowerup);
	DOREPLIFETIME(AUTCTFRoundGameState, BonusLevel);
}

void AUTCTFRoundGameState::DefaultTimer()
{
	Super::DefaultTimer();
	if (bIsAtIntermission)
	{
		IntermissionTime--;
	}
}

float AUTCTFRoundGameState::GetIntermissionTime()
{
	return IntermissionTime;
}


bool AUTCTFRoundGameState::IsTeamOnOffense(int32 TeamNumber) const
{
	const bool bIsOnRedTeam = (TeamNumber == 0);
	return (bRedToCap == bIsOnRedTeam);
}

bool AUTCTFRoundGameState::IsTeamOnDefense(int32 TeamNumber) const
{
	return !IsTeamOnOffense(TeamNumber);
}

bool AUTCTFRoundGameState::IsTeamOnDefenseNextRound(int32 TeamNumber) const 
{
	//We alternate teams, so if we are on offense now, next round we will be on defense
	return IsTeamOnOffense(TeamNumber);
}

bool AUTCTFRoundGameState::IsTeamAbleToEarnPowerup(int32 TeamNumber) const 
{
	return IsTeamOnOffense(TeamNumber) ? bIsOffenseAbleToGainPowerup : bIsDefenseAbleToGainPowerup;
}

int AUTCTFRoundGameState::GetKillsNeededForPowerup(int32 TeamNumber) const
{
	return IsTeamOnOffense(TeamNumber) ? (OffenseKillsNeededForPowerup - OffenseKills) : (DefenseKillsNeededForPowerup - DefenseKills);
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
	else if (CTFRound > 0)
	{
		if (bForScoreboard)
		{
			FFormatNamedArguments Args;
			Args.Add("RoundNum", FText::AsNumber(CTFRound));
			Args.Add("NumRounds", FText::AsNumber(NumRounds));
			return (NumRounds > 0) ? FText::Format(FullRoundInProgressStatus, Args) : FText::Format(RoundInProgressStatus, Args);
		}
		else
		{
			if (BonusLevel == 3)
			{
				return GoldBonusText;
			}
			return (BonusLevel == 2) ? SilverBonusText : BronzeBonusText;
		}
	}
	else if (IsMatchIntermission())
	{
		return IntermissionStatus;
	}

	return AUTGameState::GetGameStatusText(bForScoreboard);
}

