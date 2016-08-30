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
	GoldBonusText = NSLOCTEXT("FlagRun", "GoldBonusText", "\u2605 \u2605 \u2605");
	SilverBonusText = NSLOCTEXT("FlagRun", "SilverBonusText", "\u2605 \u2605");
	GoldBonusTimedText = NSLOCTEXT("FlagRun", "GoldBonusTimeText", "\u2605 \u2605 \u2605 {BonusTime}");
	SilverBonusTimedText = NSLOCTEXT("FlagRun", "SilverBonusTimeText", "\u2605 \u2605 {BonusTime}");
	BronzeBonusText = NSLOCTEXT("FlagRun", "BronzeBonusText", "\u2605");
	BonusLevel = 3;
	RemainingPickupDelay = 0;
	FlagRunMessageSwitch = 0;
	TiebreakValue = 0;
	FlagRunMessageTeam = nullptr;
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
	DOREPLIFETIME(AUTCTFRoundGameState, bAllowBoosts);
	DOREPLIFETIME(AUTCTFRoundGameState, bUsePrototypePowerupSelect);
	DOREPLIFETIME(AUTCTFRoundGameState, BonusLevel);
	DOREPLIFETIME(AUTCTFRoundGameState, GoldBonusThreshold);
	DOREPLIFETIME(AUTCTFRoundGameState, SilverBonusThreshold);
	DOREPLIFETIME(AUTCTFRoundGameState, RemainingPickupDelay);
	DOREPLIFETIME(AUTCTFRoundGameState, FlagRunMessageSwitch);
	DOREPLIFETIME(AUTCTFRoundGameState, FlagRunMessageTeam);
	DOREPLIFETIME(AUTCTFRoundGameState, TiebreakValue);
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
	// bonus time countdowns
	if (RemainingTime <= GoldBonusThreshold + 7)
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
		else if ((RemainingTime <= SilverBonusThreshold + 7) && (RemainingTime > SilverBonusThreshold))
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

float AUTCTFRoundGameState::GetIntermissionTime()
{
	return IntermissionTime;
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
					PC->UTClientPlaySound(SoundToPlay);
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
	if (P1->Kills < P2->Kills)
	{
		return false;
	}
	if (P1->Kills == P2->Kills)
	{
		// if score tied, use deaths to sort
		if (P1->Deaths > P2->Deaths)
			return false;

		// keep local player highest on list
		if ((P1->Deaths == P2->Deaths) && (Cast<APlayerController>(P2->GetOwner()) != NULL))
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
 
