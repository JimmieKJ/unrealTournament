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
	DOREPLIFETIME(AUTCTFRoundGameState, bUsePrototypePowerupSelect);
	DOREPLIFETIME(AUTCTFRoundGameState, BonusLevel);
	DOREPLIFETIME(AUTCTFRoundGameState, GoldBonusThreshold);
	DOREPLIFETIME(AUTCTFRoundGameState, SilverBonusThreshold);
	DOREPLIFETIME(AUTCTFRoundGameState, RemainingPickupDelay);
}

void AUTCTFRoundGameState::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld() && GetWorld()->GetAuthGameMode<AUTCTFRoundGame>())
	{
		bUsePrototypePowerupSelect = GetWorld()->GetAuthGameMode<AUTCTFRoundGame>()->bAllowPrototypePowerups;
	}

	UpdateSelectablePowerups();
	AddModeSpecificOverlays();
}

void AUTCTFRoundGameState::UpdateSelectablePowerups()
{
	const int32 RedTeamIndex = 0;
	const int32 BlueTeamIndex = 1;
	const bool bIsRedTeamOffense = IsTeamOnDefenseNextRound(RedTeamIndex);

	TSubclassOf<UUTPowerupSelectorUserWidget> OffensePowerupSelectorWidget;
	TSubclassOf<UUTPowerupSelectorUserWidget> DefensePowerupSelectorWidget;

	if (bIsRedTeamOffense)
	{
		OffensePowerupSelectorWidget = LoadClass<UUTPowerupSelectorUserWidget>(NULL, *GetPowerupSelectWidgetPath(RedTeamIndex), NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
		DefensePowerupSelectorWidget = LoadClass<UUTPowerupSelectorUserWidget>(NULL, *GetPowerupSelectWidgetPath(BlueTeamIndex), NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	}
	else
	{
		OffensePowerupSelectorWidget = LoadClass<UUTPowerupSelectorUserWidget>(NULL, *GetPowerupSelectWidgetPath(BlueTeamIndex), NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
		DefensePowerupSelectorWidget = LoadClass<UUTPowerupSelectorUserWidget>(NULL, *GetPowerupSelectWidgetPath(RedTeamIndex), NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	}
		
	if (OffensePowerupSelectorWidget)
	{
		for (TSubclassOf<class AUTInventory> BoostItem : OffensePowerupSelectorWidget.GetDefaultObject()->SelectablePowerups)
		{
			OffenseSelectablePowerups.Add(BoostItem);
		}
	}

	if (DefensePowerupSelectorWidget)
	{
		for (TSubclassOf<class AUTInventory> BoostItem : DefensePowerupSelectorWidget.GetDefaultObject()->SelectablePowerups)
		{
			DefenseSelectablePowerups.Add(BoostItem);
		}
	}
}

void AUTCTFRoundGameState::AddModeSpecificOverlays()
{
	for (TSubclassOf<class AUTInventory> BoostClass : OffenseSelectablePowerups)
	{
		BoostClass.GetDefaultObject()->AddOverlayMaterials(this);
	}

	for (TSubclassOf<class AUTInventory> BoostClass : DefenseSelectablePowerups)
	{
		BoostClass.GetDefaultObject()->AddOverlayMaterials(this);
	}
}

TSubclassOf<class AUTInventory> AUTCTFRoundGameState::GetSelectableBoostByIndex(AUTPlayerState* PlayerState, int Index) const
{
	if (PlayerState && IsTeamOnDefenseNextRound(PlayerState->GetTeamNum()))
	{
		if ((DefenseSelectablePowerups.Num() > 0) && (Index < DefenseSelectablePowerups.Num()))
		{
			return DefenseSelectablePowerups[Index];
		}
	}
	else
	{
		if ((OffenseSelectablePowerups.Num() > 0) && (Index < OffenseSelectablePowerups.Num()))
		{
			return OffenseSelectablePowerups[Index];
		}
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

FString AUTCTFRoundGameState::GetPowerupSelectWidgetPath(int32 TeamNumber)
{
	if (bUsePrototypePowerupSelect)
	{
		if (IsTeamOnDefenseNextRound(TeamNumber))
		{
			return TEXT("/Game/RestrictedAssets/Blueprints/BP_PowerupSelector_Defense_Prototype.BP_PowerupSelector_Defense_Prototype_C");
		}
		else
		{
			return TEXT("/Game/RestrictedAssets/Blueprints/BP_PowerupSelector_Offense_Prototype.BP_PowerupSelector_Offense_Prototype_C");
		}
	}
	else
	{
		if (IsTeamOnDefenseNextRound(TeamNumber))
		{
			return TEXT("/Game/RestrictedAssets/Blueprints/BP_PowerupSelector_Defense.BP_PowerupSelector_Defense_C");
		}
		else
		{
			return TEXT("/Game/RestrictedAssets/Blueprints/BP_PowerupSelector_Offense.BP_PowerupSelector_Offense_C");
		}
	}
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

