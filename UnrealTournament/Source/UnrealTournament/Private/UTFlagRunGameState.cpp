// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTFlagRunGame.h"
#include "UTFlagRunGameState.h"
#include "UTCTFGameMode.h"
#include "UTPowerupSelectorUserWidget.h"
#include "Net/UnrealNetwork.h"
#include "UTCTFScoring.h"
#include "StatNames.h"
#include "UTCountDownMessage.h"
#include "UTAnnouncer.h"
#include "UTCTFMajorMessage.h"
#include "UTRallyPoint.h"

AUTFlagRunGameState::AUTFlagRunGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRedToCap = false;
	GoldBonusText = NSLOCTEXT("FlagRun", "GoldBonusText", "\u2605 \u2605 \u2605");
	SilverBonusText = NSLOCTEXT("FlagRun", "SilverBonusText", "\u2605 \u2605");
	GoldBonusTimedText = NSLOCTEXT("FlagRun", "GoldBonusTimeText", "\u2605 \u2605 \u2605 {BonusTime}");
	SilverBonusTimedText = NSLOCTEXT("FlagRun", "SilverBonusTimeText", "\u2605 \u2605 {BonusTime}");
	BronzeBonusTimedText = NSLOCTEXT("FlagRun", "BronzeBonusTimeText", "\u2605 {BonusTime}");
	BronzeBonusText = NSLOCTEXT("FlagRun", "BronzeBonusText", "\u2605");
	BonusLevel = 3;
	bUsePrototypePowerupSelect = false;
	bAttackerLivesLimited = false;
	bDefenderLivesLimited = true;
	FlagRunMessageSwitch = 0;
	FlagRunMessageTeam = nullptr;
	bPlayStatusAnnouncements = true;
	GoldBonusColor = FLinearColor(1.f, 0.9f, 0.15f);
	SilverBonusColor = FLinearColor(0.5f, 0.5f, 0.75f);
	BronzeBonusColor = FLinearColor(0.48f, 0.25f, 0.18f);
	bEnemyRallyPointIdentified = false;
	EarlyEndTime = 0;
	bTeamGame = true;
	GoldBonusThreshold = 120;
	SilverBonusThreshold = 60;

	HighlightMap.Add(HighlightNames::MostKillsTeam, NSLOCTEXT("AUTGameMode", "MostKillsTeam", "Most Kills for Team ({0})"));
	HighlightMap.Add(HighlightNames::BadMF, NSLOCTEXT("AUTGameMode", "MostKillsTeam", "Most Kills for Team ({0})"));
	HighlightMap.Add(HighlightNames::LikeABoss, NSLOCTEXT("AUTGameMode", "MostKillsTeam", "Most Kills for Team ({0})"));
	HighlightMap.Add(HighlightNames::DeathIncarnate, NSLOCTEXT("AUTGameMode", "MostKillsTeam", "Most Kills ({0})"));

	HighlightMap.Add(HighlightNames::NaturalBornKiller, NSLOCTEXT("AUTGameMode", "LotsOfKills", "{0} Kills"));
	HighlightMap.Add(HighlightNames::SpecialForces, NSLOCTEXT("AUTGameMode", "LotsOfKills", "{0} Kills"));
	HighlightMap.Add(HighlightNames::HiredGun, NSLOCTEXT("AUTGameMode", "LotsOfKills", "{0} Kills"));
	HighlightMap.Add(HighlightNames::HappyToBeHere, NSLOCTEXT("AUTGameMode", "LotsOfKills", "{0} Kills"));

	HighlightMap.Add(HighlightNames::RedeemerRejection, NSLOCTEXT("AUTGameMode", "RedeemerRejection", "Rejected Redeemer"));
	HighlightMap.Add(HighlightNames::FlagDenials, NSLOCTEXT("AUTGameMode", "FlagDenials", "{0} Denials"));
	HighlightMap.Add(HighlightNames::WeaponKills, NSLOCTEXT("AUTGameMode", "WeaponKills", "{0} kills with {1}"));
	HighlightMap.Add(HighlightNames::KillingBlowsAward, NSLOCTEXT("AUTGameMode", "KillingBlowsAward", "{0} killing blows"));
	HighlightMap.Add(HighlightNames::MostKillingBlowsAward, NSLOCTEXT("AUTGameMode", "MostKillingBlowsAward", "Most killing blows ({0})"));

	ShortHighlightMap.Add(HighlightNames::BadMF, NSLOCTEXT("AUTGameMode", "BadMF", "Bad MotherF**ker"));
	ShortHighlightMap.Add(HighlightNames::LikeABoss, NSLOCTEXT("AUTGameMode", "LikeABoss", "Like a Boss"));
	ShortHighlightMap.Add(HighlightNames::DeathIncarnate, NSLOCTEXT("AUTGameMode", "DeathIncarnate", "Death Incarnate"));
	ShortHighlightMap.Add(HighlightNames::NaturalBornKiller, NSLOCTEXT("AUTGameMode", "NaturalBornKiller", "Natural Born Killer"));
	ShortHighlightMap.Add(HighlightNames::SpecialForces, NSLOCTEXT("AUTGameMode", "SpecialForces", "Special Forces"));
	ShortHighlightMap.Add(HighlightNames::HiredGun, NSLOCTEXT("AUTGameMode", "HiredGun", "Hired Gun"));
	ShortHighlightMap.Add(HighlightNames::HappyToBeHere, NSLOCTEXT("AUTGameMode", "HappyToBeHere", "Just Happy To Be Here"));
	ShortHighlightMap.Add(HighlightNames::MostKillsTeam, NSLOCTEXT("AUTGameMode", "ShortMostKills", "Top Gun"));

	ShortHighlightMap.Add(HighlightNames::RedeemerRejection, NSLOCTEXT("AUTGameMode", "ShortRejection", "Redeem this"));
	ShortHighlightMap.Add(HighlightNames::FlagDenials, NSLOCTEXT("AUTGameMode", "ShortDenials", "You shall not pass"));
	ShortHighlightMap.Add(HighlightNames::WeaponKills, NSLOCTEXT("AUTGameMode", "ShortWeaponKills", "Weapon Master"));
	ShortHighlightMap.Add(HighlightNames::KillingBlowsAward, NSLOCTEXT("AUTGameMode", "ShortKillingBlowsAward", "Nice Shot"));
	ShortHighlightMap.Add(HighlightNames::MostKillingBlowsAward, NSLOCTEXT("AUTGameMode", "ShortMostKillingBlowsAward", "Punisher"));
}

void AUTFlagRunGameState::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld() && GetWorld()->GetAuthGameMode<AUTFlagRunGame>())
	{
		bUsePrototypePowerupSelect = GetWorld()->GetAuthGameMode<AUTFlagRunGame>()->bAllowPrototypePowerups;
		bAllowBoosts = GetWorld()->GetAuthGameMode<AUTFlagRunGame>()->bAllowBoosts;
	}

	UpdateSelectablePowerups();
	AddModeSpecificOverlays();
}

void AUTFlagRunGameState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTFlagRunGameState, bRedToCap);
	DOREPLIFETIME(AUTFlagRunGameState, BonusLevel);
	DOREPLIFETIME(AUTFlagRunGameState, CurrentRallyPoint);
	DOREPLIFETIME(AUTFlagRunGameState, bEnemyRallyPointIdentified);
	DOREPLIFETIME(AUTFlagRunGameState, FlagRunMessageSwitch);
	DOREPLIFETIME(AUTFlagRunGameState, FlagRunMessageTeam);
	DOREPLIFETIME(AUTFlagRunGameState, bAttackersCanRally);
	DOREPLIFETIME(AUTFlagRunGameState, EarlyEndTime);
	DOREPLIFETIME(AUTFlagRunGameState, bAllowBoosts);
	DOREPLIFETIME(AUTFlagRunGameState, bUsePrototypePowerupSelect);
	DOREPLIFETIME(AUTFlagRunGameState, OffenseKillsNeededForPowerup);
	DOREPLIFETIME(AUTFlagRunGameState, DefenseKillsNeededForPowerup);
	DOREPLIFETIME(AUTFlagRunGameState, bIsDefenseAbleToGainPowerup);
	DOREPLIFETIME(AUTFlagRunGameState, bIsOffenseAbleToGainPowerup);
	DOREPLIFETIME(AUTFlagRunGameState, OffenseSelectablePowerups);
	DOREPLIFETIME(AUTFlagRunGameState, DefenseSelectablePowerups);
}

void AUTFlagRunGameState::OnIntermissionChanged()
{
	// FIXMESTEVE don't need this or super once clean up CTF intermission.
}

void AUTFlagRunGameState::OnBonusLevelChanged()
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

void AUTFlagRunGameState::UpdateTimeMessage()
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

FLinearColor AUTFlagRunGameState::GetGameStatusColor()
{
	if (CTFRound > 0)
	{
		switch(BonusLevel)
		{
			case 1: return BronzeBonusColor; break;
			case 2: return SilverBonusColor; break;
			case 3: return GoldBonusColor; break;
		}
	}
	UE_LOG(UT, Warning, TEXT("WHITE"));
	return FLinearColor::White;
}

FText AUTFlagRunGameState::GetRoundStatusText(bool bForScoreboard)
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
		FText StatusText = BronzeBonusTimedText;
		int32 RemainingBonus = FMath::Clamp(RemainingTime, 0, 59);
		if (BonusLevel == 3)
		{
			RemainingBonus = FMath::Clamp(RemainingTime - GoldBonusThreshold, 0, 60);
			StatusText = GoldBonusTimedText;
			FFormatNamedArguments Args;
			Args.Add("BonusTime", FText::AsNumber(RemainingBonus));
			return FText::Format(GoldBonusTimedText, Args);
		}
		else if (BonusLevel == 2)
		{
			RemainingBonus = FMath::Clamp(RemainingTime - SilverBonusThreshold, 0, 59);
			StatusText = SilverBonusTimedText;
		}
		FFormatNamedArguments Args;
		Args.Add("BonusTime", FText::AsNumber(RemainingBonus));
		return FText::Format(StatusText, Args);
	}
}

void AUTFlagRunGameState::UpdateSelectablePowerups()
{
	if (!bAllowBoosts)
	{
		OffenseSelectablePowerups.Empty();
		DefenseSelectablePowerups.Empty();
		return;
	}
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

void AUTFlagRunGameState::SetSelectablePowerups(const TArray<TSubclassOf<AUTInventory>>& OffenseList, const TArray<TSubclassOf<AUTInventory>>& DefenseList)
{
	OffenseSelectablePowerups = OffenseList;
	DefenseSelectablePowerups = DefenseList;
	for (int32 i = 0; i < PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerArray[i]);
		if (PS != nullptr && PS->BoostClass != nullptr && !OffenseList.Contains(PS->BoostClass) && !DefenseList.Contains(PS->BoostClass))
		{
			PS->ServerSetBoostItem(0);
		}
	}
}

FString AUTFlagRunGameState::GetPowerupSelectWidgetPath(int32 TeamNumber)
{
	if (!bAllowBoosts)
	{
		if (IsTeamOnDefenseNextRound(TeamNumber))
		{
			return TEXT("/Game/RestrictedAssets/Blueprints/BP_PowerupSelector_EmptyDefense.BP_PowerupSelector_EmptyDefense_C");
		}
		else
		{
			return TEXT("/Game/RestrictedAssets/Blueprints/BP_PowerupSelector_EmptyOffense.BP_PowerupSelector_EmptyOffense_C");
		}
	}
	else if (bUsePrototypePowerupSelect)
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

void AUTFlagRunGameState::AddModeSpecificOverlays()
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

TSubclassOf<class AUTInventory> AUTFlagRunGameState::GetSelectableBoostByIndex(AUTPlayerState* PlayerState, int Index) const
{
	if (PlayerState != nullptr && (IsMatchInProgress() ? IsTeamOnDefense(PlayerState->GetTeamNum()) : IsTeamOnDefenseNextRound(PlayerState->GetTeamNum())))
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

bool AUTFlagRunGameState::IsSelectedBoostValid(AUTPlayerState* PlayerState) const
{
	if (PlayerState == nullptr || PlayerState->BoostClass == nullptr)
	{
		return false;
	}

	return IsTeamOnDefenseNextRound(PlayerState->GetTeamNum()) ? DefenseSelectablePowerups.Contains(PlayerState->BoostClass) : OffenseSelectablePowerups.Contains(PlayerState->BoostClass);
}

void AUTFlagRunGameState::PrecacheAllPowerupAnnouncements(class UUTAnnouncer* Announcer) const
{
	for (TSubclassOf<class AUTInventory> PowerupClass : DefenseSelectablePowerups)
	{
		CachePowerupAnnouncement(Announcer, PowerupClass);
	}

	for (TSubclassOf<class AUTInventory> PowerupClass : OffenseSelectablePowerups)
	{
		CachePowerupAnnouncement(Announcer, PowerupClass);
	}
}

void AUTFlagRunGameState::CachePowerupAnnouncement(class UUTAnnouncer* Announcer, const TSubclassOf<AUTInventory> PowerupClass) const
{
	AUTInventory* Powerup = PowerupClass->GetDefaultObject<AUTInventory>();
	if (Powerup)
	{
		Announcer->PrecacheAnnouncement(Powerup->AnnouncementName);
	}
}

bool AUTFlagRunGameState::IsTeamAbleToEarnPowerup(int32 TeamNumber) const
{
	return IsTeamOnOffense(TeamNumber) ? bIsOffenseAbleToGainPowerup : bIsDefenseAbleToGainPowerup;
}

AUTCTFFlag* AUTFlagRunGameState::GetOffenseFlag()
{
	int OffenseTeam = bRedToCap ? 0 : 1;
	return ((FlagBases.Num() > OffenseTeam) ? FlagBases[OffenseTeam]->MyFlag : nullptr);
}

int AUTFlagRunGameState::GetKillsNeededForPowerup(int32 TeamNumber) const
{
	return IsTeamOnOffense(TeamNumber) ? (OffenseKillsNeededForPowerup - OffenseKills) : (DefenseKillsNeededForPowerup - DefenseKills);
}

void AUTFlagRunGameState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Role == ROLE_Authority)
	{
		uint8 OffensiveTeam = bRedToCap ? 0 : 1;
		if (FlagBases.IsValidIndex(OffensiveTeam) && FlagBases[OffensiveTeam] != nullptr)
		{
			AUTCTFFlag* Flag = Cast<AUTCTFFlag>(FlagBases[OffensiveTeam]->GetCarriedObject());
			bAttackersCanRally = (CurrentRallyPoint != nullptr) && (CurrentRallyPoint->RallyPointState == RallyPointStates::Powered);
			AUTGameVolume* GV = Flag && Flag->HoldingPawn && Flag->HoldingPawn->UTCharacterMovement ? Cast<AUTGameVolume>(Flag->HoldingPawn->UTCharacterMovement->GetPhysicsVolume()) : nullptr;
			bool bInFlagRoom = GV && (GV->bIsNoRallyZone || GV->bIsTeamSafeVolume);
			bHaveEstablishedFlagRunner = (!bInFlagRoom && Flag && Flag->Holder && Flag->HoldingPawn && (GetWorld()->GetTimeSeconds() - Flag->PickedUpTime > 2.f));
		}
	}
}

bool AUTFlagRunGameState::IsTeamOnOffense(int32 TeamNumber) const
{
	const bool bIsOnRedTeam = (TeamNumber == 0);
	return (bRedToCap == bIsOnRedTeam);
}

bool AUTFlagRunGameState::IsTeamOnDefense(int32 TeamNumber) const
{
	return !IsTeamOnOffense(TeamNumber);
}

bool AUTFlagRunGameState::IsTeamOnDefenseNextRound(int32 TeamNumber) const
{
	//We alternate teams, so if we are on offense now, next round we will be on defense
	return IsTeamOnOffense(TeamNumber);
}

void AUTFlagRunGameState::CheckTimerMessage()
{
	RemainingTime -= EarlyEndTime;
	Super::CheckTimerMessage();
	RemainingTime += EarlyEndTime;
}

int32 AUTFlagRunGameState::NumHighlightsNeeded()
{
	return HasMatchEnded() ? 3 : 1;
}

void AUTFlagRunGameState::AddMinorHighlights_Implementation(AUTPlayerState* PS)
{
	if (HasMatchEnded())
	{
		if (PS->GetRoundStatsValue(NAME_FCKills) > 0)
		{
			PS->AddMatchHighlight(NAME_FCKills, PS->GetRoundStatsValue(NAME_FCKills));
			if (PS->MatchHighlights[NumHighlightsNeeded() - 1] != NAME_None)
			{
				return;
			}
		}
		AUTGameState::AddMinorHighlights_Implementation(PS);
		return;
	}
	if (PS->MatchHighlights[0] != NAME_None)
	{
		return;
	}

	// sprees and multikills
	FName SpreeStatsNames[5] = { NAME_SpreeKillLevel4, NAME_SpreeKillLevel3, NAME_SpreeKillLevel2, NAME_SpreeKillLevel1, NAME_SpreeKillLevel0 };
	for (int32 i = 0; i < 5; i++)
	{
		if (PS->GetStatsValue(SpreeStatsNames[i]) > 0)
		{
			PS->AddMatchHighlight(SpreeStatsNames[i], PS->GetRoundStatsValue(SpreeStatsNames[i]));
			return;
		}
	}

	if (PS->RoundKills + PS->RoundKillAssists >= 15)
	{
		PS->MatchHighlights[0] = HighlightNames::NaturalBornKiller;
		PS->MatchHighlightData[0] = PS->RoundKills + PS->RoundKillAssists;
		return;
	}

	FName MultiKillsNames[4] = { NAME_MultiKillLevel3, NAME_MultiKillLevel2, NAME_MultiKillLevel1, NAME_MultiKillLevel0 };
	for (int32 i = 0; i < 2; i++)
	{
		if (PS->GetRoundStatsValue(MultiKillsNames[i]) > 0)
		{
			PS->AddMatchHighlight(MultiKillsNames[i], PS->GetRoundStatsValue(MultiKillsNames[i]));
			return;
		}
	}
	if (PS->RoundKills + PS->RoundKillAssists >= 10)
	{
		PS->MatchHighlights[0] = HighlightNames::SpecialForces;
		PS->MatchHighlightData[0] = PS->RoundKills + PS->RoundKillAssists;
		return;
	}

	// announced kills
	FName AnnouncedKills[5] = { NAME_AmazingCombos, NAME_AirRox, NAME_AirSnot, NAME_SniperHeadshotKills, NAME_FlakShreds };
	for (int32 i = 0; i < 5; i++)
	{
		if (PS->GetRoundStatsValue(AnnouncedKills[i]) > 1)
		{
			PS->AddMatchHighlight(AnnouncedKills[i], PS->GetRoundStatsValue(AnnouncedKills[i]));
			return;
		}
	}

	// Most kills with favorite weapon, if needed
	bool bHasMultipleKillWeapon = false;
	if (PS->FavoriteWeapon)
	{
		AUTWeapon* DefaultWeapon = PS->FavoriteWeapon->GetDefaultObject<AUTWeapon>();
		int32 WeaponKills = DefaultWeapon->GetWeaponKillStatsForRound(PS);
		if (WeaponKills > 1)
		{
			bHasMultipleKillWeapon = true;
			bool bIsBestOverall = true;
			for (int32 i = 0; i < PlayerArray.Num(); i++)
			{
				AUTPlayerState* OtherPS = Cast<AUTPlayerState>(PlayerArray[i]);
				if (OtherPS && (PS != OtherPS) && (DefaultWeapon->GetWeaponKillStatsForRound(OtherPS) > WeaponKills))
				{
					bIsBestOverall = false;
					break;
				}
			}
			if (bIsBestOverall)
			{
				PS->AddMatchHighlight(HighlightNames::MostWeaponKills, WeaponKills);
				return;
			}
		}
	}

	// flag cap FIXMESTEVE

	if (bHasMultipleKillWeapon)
	{
		AUTWeapon* DefaultWeapon = PS->FavoriteWeapon->GetDefaultObject<AUTWeapon>();
		int32 WeaponKills = DefaultWeapon->GetWeaponKillStatsForRound(PS);
		PS->AddMatchHighlight(HighlightNames::WeaponKills, WeaponKills);
	}
	else if (PS->GetRoundStatsValue(NAME_FCKills) > 1)
	{
		PS->AddMatchHighlight(NAME_FCKills, PS->GetStatsValue(NAME_FCKills));
	}
	else if (PS->RoundKills > FMath::Max(1, PS->RoundKillAssists))
	{
		PS->MatchHighlights[0] = HighlightNames::KillingBlowsAward;
		PS->MatchHighlightData[0] = PS->RoundKills;
	}
	else if (PS->RoundKills + PS->RoundKillAssists > 0)
	{
		PS->MatchHighlights[0] = (PS->RoundKills + PS->RoundKillAssists > 2) ? HighlightNames::HiredGun : HighlightNames::HappyToBeHere;
		PS->MatchHighlightData[0] = PS->RoundKills + PS->RoundKillAssists;
	}
	else if (PS->RoundDamageDone > 0)
	{
		PS->MatchHighlights[0] = HighlightNames::HappyToBeHere;
		PS->MatchHighlightData[0] = PS->RoundDamageDone;
	}
	else
	{
		PS->MatchHighlights[0] = HighlightNames::ParticipationAward;
	}
}

// new plan - rank order kills, give pending award.. Early out if good enough, override for lower
void AUTFlagRunGameState::UpdateHighlights_Implementation()
{
	if (HasMatchEnded())
	{
		AUTGameState::UpdateHighlights_Implementation();
		return;
	}

	//Collect all the weapons
	TArray<AUTWeapon *> StatsWeapons;
	if (StatsWeapons.Num() == 0)
	{
		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AUTPickupWeapon* Pickup = Cast<AUTPickupWeapon>(*It);
			if (Pickup && Pickup->GetInventoryType())
			{
				StatsWeapons.AddUnique(Pickup->GetInventoryType()->GetDefaultObject<AUTWeapon>());
			}
		}
	}

	AUTPlayerState* MostKills = NULL;
	AUTPlayerState* MostKillsRed = NULL;
	AUTPlayerState* MostKillsBlue = NULL;
	AUTPlayerState* MostHeadShotsPS = NULL;
	AUTPlayerState* MostAirRoxPS = NULL;
	AUTPlayerState* MostKillingBlowsRed = NULL;
	AUTPlayerState* MostKillingBlowsBlue = NULL;

	for (TActorIterator<AUTPlayerState> It(GetWorld()); It; ++It)
	{
		AUTPlayerState* PS = *It;
		if (PS && !PS->bOnlySpectator)
		{
			int32 TeamIndex = PS->Team ? PS->Team->TeamIndex : 0;
			int32 TotalKills = PS->RoundKills + PS->RoundKillAssists;
			if (TotalKills > (MostKills ? MostKills->RoundKills + MostKills->RoundKillAssists : 0))
			{
				MostKills = PS;
			}
			AUTPlayerState* TopTeamKiller = (TeamIndex == 0) ? MostKillsRed : MostKillsBlue;
			if (TotalKills > (TopTeamKiller ? TopTeamKiller->RoundKills + TopTeamKiller->RoundKillAssists : 0))
			{
				if (TeamIndex == 0)
				{
					MostKillsRed = PS;
				}
				else
				{
					MostKillsBlue = PS;
				}
			}
			AUTPlayerState* TopKillingBlows = (TeamIndex == 0) ? MostKillingBlowsRed : MostKillingBlowsBlue;
			if (PS->RoundKills > (TopKillingBlows ? TopKillingBlows->RoundKills : 0))
			{
				if (TeamIndex == 0)
				{
					MostKillingBlowsRed = PS;
				}
				else
				{
					MostKillingBlowsBlue = PS;
				}
			}

			//Figure out what weapon killed the most
			PS->FavoriteWeapon = nullptr;
			int32 BestKills = 0;
			for (AUTWeapon* Weapon : StatsWeapons)
			{
				int32 Kills = Weapon->GetWeaponKillStatsForRound(PS);
				if (Kills > BestKills)
				{
					BestKills = Kills;
					PS->FavoriteWeapon = Weapon->GetClass();
				}
			}

			if (PS->GetRoundStatsValue(NAME_SniperHeadshotKills) > (MostHeadShotsPS ? MostHeadShotsPS->GetRoundStatsValue(NAME_SniperHeadshotKills) : 0.f))
			{
				MostHeadShotsPS = PS;
			}
			if (PS->GetRoundStatsValue(NAME_AirRox) > (MostAirRoxPS ? MostAirRoxPS->GetRoundStatsValue(NAME_AirRox) : 0.f))
			{
				MostAirRoxPS = PS;
			}
		}
	}

	for (TActorIterator<AUTPlayerState> It(GetWorld()); It; ++It)
	{
		AUTPlayerState* PS = *It;
		if (PS && !PS->bOnlySpectator)
		{
			int32 TotalKills = PS->RoundKills + PS->RoundKillAssists;
			if (MostKills && (TotalKills >= 20) && (TotalKills >= MostKills->RoundKills + MostKills->RoundKillAssists))
			{
				PS->AddMatchHighlight(HighlightNames::DeathIncarnate, MostKills->Kills);
			}
			else if (PS->Team)
			{
				AUTPlayerState* TopTeamKiller = (PS->Team->TeamIndex == 0) ? MostKillsRed : MostKillsBlue;
				if (TopTeamKiller && (TotalKills >= TopTeamKiller->RoundKills + TopTeamKiller->RoundKillAssists))
				{
					if (TotalKills >= 15)
					{
						PS->AddMatchHighlight(HighlightNames::BadMF, TotalKills);
					}
					else if (TotalKills >= 10)
					{
						PS->AddMatchHighlight(HighlightNames::LikeABoss, TotalKills);
					}
					else
					{
						PS->AddMatchHighlight(HighlightNames::MostKillsTeam, TotalKills);
					}
				}
				else
				{
					AUTPlayerState* TopKillingBlows = (PS->Team->TeamIndex == 0) ? MostKillingBlowsRed : MostKillingBlowsBlue;
					if (TopKillingBlows && (PS->RoundKills >= TopKillingBlows->RoundKills))
					{
						PS->AddMatchHighlight(HighlightNames::MostKillingBlowsAward, PS->RoundKills);
					}
				}
			}
			if (PS->MatchHighlights[0] == NAME_None)
			{
				if (PS->GetRoundStatsValue(NAME_FlagDenials) > 1)
				{
					PS->AddMatchHighlight(HighlightNames::FlagDenials, PS->GetRoundStatsValue(NAME_FlagDenials));
				}
				else if (PS->GetRoundStatsValue(NAME_RedeemerRejected) > 0)
				{
					PS->AddMatchHighlight(HighlightNames::RedeemerRejection, PS->GetRoundStatsValue(NAME_RedeemerRejected));
				}
			}
		}
	}

	for (TActorIterator<AUTPlayerState> It(GetWorld()); It; ++It)
	{
		AUTPlayerState* PS = *It;
		if (PS && !PS->bOnlySpectator && PS->MatchHighlights[0] == NAME_None)
		{
			if (MostHeadShotsPS && (PS->GetRoundStatsValue(NAME_SniperHeadshotKills) == MostHeadShotsPS->GetRoundStatsValue(NAME_SniperHeadshotKills)))
			{
				PS->AddMatchHighlight(HighlightNames::MostHeadShots, MostHeadShotsPS->GetRoundStatsValue(NAME_SniperHeadshotKills));
			}
			else if (MostAirRoxPS && (PS->GetRoundStatsValue(NAME_AirRox) == MostAirRoxPS->GetRoundStatsValue(NAME_AirRox)))
			{
				PS->AddMatchHighlight(HighlightNames::MostAirRockets, MostAirRoxPS->GetRoundStatsValue(NAME_AirRox));
			}
		}
	}

	for (TActorIterator<AUTPlayerState> It(GetWorld()); It; ++It)
	{
		AUTPlayerState* PS = *It;
		if (PS && !PS->bOnlySpectator && PS->MatchHighlights[0] == NAME_None)
		{
			// only add low priority highlights if not enough high priority highlights
			AddMinorHighlights(PS);
		}
	}
}



