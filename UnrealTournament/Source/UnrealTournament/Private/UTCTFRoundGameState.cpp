// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFRoundGameState.h"
#include "UTCTFGameMode.h"
#include "Net/UnrealNetwork.h"
#include "UTCTFScoring.h"
#include "StatNames.h"
#include "UTCountDownMessage.h"

AUTCTFRoundGameState::AUTCTFRoundGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GoldBonusText = NSLOCTEXT("FlagRun", "GoldBonusText", "GOLD");
	SilverBonusText = NSLOCTEXT("FlagRun", "SilverBonusText", "SILVER");
	GoldBonusTimedText = NSLOCTEXT("FlagRun", "GoldBonusTimeText", "GOLD {BonusTime}");
	SilverBonusTimedText = NSLOCTEXT("FlagRun", "SilverBonusTimeText", "SILVER {BonusTime}");
	BronzeBonusText = NSLOCTEXT("FlagRun", "BronzeBonusText", "BRONZE");
	BonusLevel = 3;
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
	DOREPLIFETIME(AUTCTFRoundGameState, GoldBonusThreshold);
	DOREPLIFETIME(AUTCTFRoundGameState, SilverBonusThreshold);
}

void AUTCTFRoundGameState::DefaultTimer()
{
	Super::DefaultTimer();
	if (bIsAtIntermission)
	{
		IntermissionTime--;
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		// bonus time countdowns
		if (RemainingTime <= GoldBonusThreshold + 5)
		{
			if (RemainingTime > GoldBonusThreshold)
			{
				for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
				{
					AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
					if (PC != NULL)
					{
						PC->ClientReceiveLocalizedMessage(UUTCountDownMessage::StaticClass(), 4000 + RemainingTime - GoldBonusThreshold);
					}
				}
			}
			else if ((RemainingTime <= SilverBonusThreshold + 5) && (RemainingTime > SilverBonusThreshold))
			{
				for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
				{
					AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
					if (PC != NULL)
					{
						PC->ClientReceiveLocalizedMessage(UUTCountDownMessage::StaticClass(), 3000 + RemainingTime - SilverBonusThreshold);
					}
				}
			}
		}
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

void AUTCTFRoundGameState::OnBonusLevelChanged()
{
	if (BonusLevel < 3)
	{
		USoundBase* SoundToPlay = UUTCountDownMessage::StaticClass()->GetDefaultObject<UUTCountDownMessage>()->TimeEndingSound;
		if (SoundToPlay != NULL)
		{
			for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
				if (PC && PC->IsLocalPlayerController())
				{
					PC->ClientPlaySound(SoundToPlay);
				}
			}
		}
	}
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
				int32 RemainingBonus = FMath::Max(0,RemainingTime - GoldBonusThreshold);
				if (RemainingBonus < 30)
				{ 
					FFormatNamedArguments Args;
					Args.Add("BonusTime", FText::AsNumber(RemainingBonus));
					return FText::Format(GoldBonusTimedText, Args);
				}
				return GoldBonusText;
			}
			else if (BonusLevel == 2)
			{
				int32 RemainingBonus = FMath::Max(0, RemainingTime - SilverBonusThreshold);
				if (RemainingBonus < 30)
				{
					FFormatNamedArguments Args;
					Args.Add("BonusTime", FText::AsNumber(RemainingBonus));
					return FText::Format(SilverBonusTimedText, Args);
				}
				return SilverBonusText;
			}
			return BronzeBonusText;
		}
	}
	else if (IsMatchIntermission())
	{
		return IntermissionStatus;
	}

	return AUTGameState::GetGameStatusText(bForScoreboard);
}

