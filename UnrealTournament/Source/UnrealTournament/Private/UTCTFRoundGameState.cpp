// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFRoundGameState.h"
#include "UTCTFGameMode.h"
#include "UTPowerupSelectorUserWidget.h"
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
	RemainingPickupDelay = 0;
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
	DOREPLIFETIME(AUTCTFRoundGameState, RemainingPickupDelay);
}

void AUTCTFRoundGameState::BeginPlay()
{
	Super::BeginPlay();

	UpdateSelectablePowerups();
	AddModeSpecificOverlays();
}

void AUTCTFRoundGameState::UpdateSelectablePowerups()
{
	TSubclassOf<UUTPowerupSelectorUserWidget> PowerupSelectorWidget = LoadClass<UUTPowerupSelectorUserWidget>(NULL, TEXT("/Game/RestrictedAssets/Blueprints/BP_PowerupSelector_Defense.BP_PowerupSelector_Defense_C"), NULL, LOAD_NoWarn | LOAD_Quiet, NULL);

	if (PowerupSelectorWidget)
	{
		for (TSubclassOf<class AUTInventory> BoostItem : PowerupSelectorWidget.GetDefaultObject()->SelectablePowerups)
		{
			SelectablePowerups.Add(BoostItem);
		}
	}
}

void AUTCTFRoundGameState::AddModeSpecificOverlays()
{
	for (TSubclassOf<class AUTInventory> BoostClass : SelectablePowerups)
	{
		BoostClass.GetDefaultObject()->AddOverlayMaterials(this);
	}
}

TSubclassOf<class AUTInventory> AUTCTFRoundGameState::GetSelectableBoostByIndex(AUTPlayerState* PlayerState, int Index) const
{
	if ((SelectablePowerups.Num() > 0) && (Index < SelectablePowerups.Num()))
	{
		return SelectablePowerups[Index];
	}

	return nullptr;
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

