// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTFlagRunGame.h"
#include "UTCTFGameMessage.h"
#include "UTCTFRoleMessage.h"
#include "UTCTFRewardMessage.h"
#include "UTCTFMajorMessage.h"
#include "UTFirstBloodMessage.h"
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
#include "UTShowdownRewardMessage.h"
#include "UTPlayerStart.h"
#include "UTArmor.h"
#include "UTTimedPowerup.h"
#include "UTPlayerState.h"
#include "UTFlagRunHUD.h"
#include "UTGhostFlag.h"
#include "UTFlagRunGameState.h"
#include "UTAsymCTFSquadAI.h"
#include "UTWeaponRedirector.h"
#include "UTWeaponLocker.h"
#include "UTFlagRunMessage.h"
#include "UTWeap_Translocator.h"
#include "UTReplicatedEmitter.h"
#include "UTATypes.h"
#include "UTGameVolume.h"
#include "UTTaunt.h"
#include "Animation/AnimInstance.h"
#include "UTFlagRunGameMessage.h"
#include "UTAnalytics.h"
#include "UTRallyPoint.h"
#include "UTRemoteRedeemer.h"
#include "UTFlagRunScoring.h"

AUTFlagRunGame::AUTFlagRunGame(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MinPlayersToStart = 10;
	GoldScore = 3;
	SilverScore = 2;
	BronzeScore = 1;
	DefenseScore = 1;
	DisplayName = NSLOCTEXT("UTGameMode", "FLAGRUN", "Flag Run");
	bHideInUI = false;
	bWeaponStayActive = false;
	bAllowPickupAnnouncements = true;
	LastEntryDefenseWarningTime = 0.f;
	MapPrefix = TEXT("FR");
	GameStateClass = AUTFlagRunGameState::StaticClass();
	bAllowBoosts = false;
	OffenseKillsNeededForPowerUp = 10;
	DefenseKillsNeededForPowerUp = 10;
	bCarryOwnFlag = true;
	bNoFlagReturn = true;
	bGameHasImpactHammer = false;
	FlagPickupDelay = 15;
	bTrackKillAssists = true;
	CTFScoringClass = AUTFlagRunScoring::StaticClass();

	ActivatedPowerupPlaceholderObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Powerups/BP_ActivatedPowerup_UDamage.BP_ActivatedPowerup_UDamage_C"));
	RepulsorObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Powerups/BP_Repulsor.BP_Repulsor_C"));

	static ConstructorHelpers::FObjectFinder<UClass> AfterImageFinder(TEXT("Blueprint'/Game/RestrictedAssets/Weapons/Translocator/TransAfterImage.TransAfterImage_C'"));
	AfterImageType = AfterImageFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> RallyFinalSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/RallyFailed.RallyFailed'"));
	RallyFailedSound = RallyFinalSoundFinder.Object;
}

void AUTFlagRunGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (!ActivatedPowerupPlaceholderObject.IsNull())
	{
		ActivatedPowerupPlaceholderClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *ActivatedPowerupPlaceholderObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}
	if (!RepulsorObject.IsNull())
	{
		RepulsorClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *RepulsorObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}

	OffenseKillsNeededForPowerUp = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("OffKillsForPowerup"), OffenseKillsNeededForPowerUp));
	DefenseKillsNeededForPowerUp = FMath::Max(0, UGameplayStatics::GetIntOption(Options, TEXT("DefKillsForPowerup"), DefenseKillsNeededForPowerUp));

	FString InOpt = UGameplayStatics::ParseOption(Options, TEXT("AllowPrototypePowerups"));
	bAllowPrototypePowerups = EvalBoolOptions(InOpt, bAllowPrototypePowerups);

	InOpt = UGameplayStatics::ParseOption(Options, TEXT("Boost"));
	bAllowBoosts = EvalBoolOptions(InOpt, bAllowBoosts);
	if (!bAllowBoosts)
	{
		OffenseKillsNeededForPowerUp = 1000;
		DefenseKillsNeededForPowerUp = 1000;
	}
	GameSession->MaxPlayers = 10;

	if (bDevServer)
	{
		FlagPickupDelay = 3.f;
	}
}

int32 AUTFlagRunGame::GetFlagCapScore()
{
	AUTFlagRunGameState* GS = Cast<AUTFlagRunGameState>(UTGameState);
	if (GS)
	{
		int32 BonusTime = GS->GetRemainingTime();
		if (BonusTime >= GS->GoldBonusThreshold)
		{
			return GoldScore;
		}
		if (BonusTime >= GS->SilverBonusThreshold)
		{
			return SilverScore;
		}
	}
	return BronzeScore;
}

void AUTFlagRunGame::AnnounceWin(AUTTeamInfo* WinningTeam, APlayerState* ScoringPlayer, uint8 Reason)
{
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(*Iterator);
		if (UTPC)
		{
			UTPC->ClientAnnounceRoundScore(WinningTeam, ScoringPlayer, WinningTeam->RoundBonus, Reason);
		}
	}
}

void AUTFlagRunGame::BroadcastCTFScore(APlayerState* ScoringPlayer, AUTTeamInfo* ScoringTeam, int32 OldScore)
{
	AnnounceWin(ScoringTeam, ScoringPlayer, 0);
}

void AUTFlagRunGame::InitGameStateForRound()
{
	Super::InitGameStateForRound();
	AUTFlagRunGameState* FRGS = Cast<AUTFlagRunGameState>(CTFGameState);
	if (FRGS)
	{
		FRGS->OffenseKills = 0;
		FRGS->DefenseKills = 0;
		FRGS->OffenseKillsNeededForPowerup = OffenseKillsNeededForPowerUp;
		FRGS->DefenseKillsNeededForPowerup = DefenseKillsNeededForPowerUp;
		FRGS->bIsOffenseAbleToGainPowerup = true;
		FRGS->bIsDefenseAbleToGainPowerup = true;
		FRGS->bRedToCap = !FRGS->bRedToCap;
		FRGS->CurrentRallyPoint = nullptr;
		FRGS->bEnemyRallyPointIdentified = false;
		FRGS->ScoringPlayerState = nullptr;
	}
}

float AUTFlagRunGame::RatePlayerStart(APlayerStart* P, AController* Player)
{
	// @TODO FIXMESTEVE no need to check enemy traces, just overlaps
	float Result = Super::RatePlayerStart(P, Player);
	if ((Result > 0.f) && Cast<AUTPlayerStart>(P))
	{
		// try to spread out spawns between volumes
		AUTPlayerStart* Start = (AUTPlayerStart*)P;
		AUTPlayerState* PS = Player ? Cast<AUTPlayerState>(Player->PlayerState) : nullptr;
		AUTFlagRunGameState* FRGS = Cast<AUTFlagRunGameState>(CTFGameState);
		if (Start && PS && FRGS && PS->Team != NULL)
		{
			const bool bIsAttacker = (FRGS->bRedToCap == (PS->Team->TeamIndex == 0));
			if (Start->PlayerStartGroup != (bIsAttacker ? LastAttackerSpawnGroup : LastDefenderSpawnGroup))
			{
				Result += 20.f;
			}
		}
	}
	return Result;
}

AActor* AUTFlagRunGame::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	AActor* const Best = Super::FindPlayerStart_Implementation(Player, IncomingName);
	if (Best)
	{
		LastStartSpot = Best;
		AUTPlayerStart* Start = (AUTPlayerStart*)Best;
		AUTPlayerState* PS = Player ? Cast<AUTPlayerState>(Player->PlayerState) : nullptr;
		AUTFlagRunGameState* FRGS = Cast<AUTFlagRunGameState>(CTFGameState);
		if (Start && PS && FRGS && PS->Team != NULL)
		{
			if (FRGS->bRedToCap == (PS->Team->TeamIndex == 0))
			{
				LastAttackerSpawnGroup = Start->PlayerStartGroup;
			}
			else
			{
				LastDefenderSpawnGroup = Start->PlayerStartGroup;
			}
		}
	}
	return Best;
}

bool AUTFlagRunGame::AvoidPlayerStart(AUTPlayerStart* P)
{
	return P && P->bIgnoreInASymCTF;
}

int32 AUTFlagRunGame::GetDefenseScore()
{
	return DefenseScore;
}

void AUTFlagRunGame::CheckRoundTimeVictory()
{
	AUTFlagRunGameState* FRGS = Cast<AUTFlagRunGameState>(CTFGameState);
	int32 RemainingTime = UTGameState ? UTGameState->GetRemainingTime() : 100;
	if (RemainingTime <= 0)
	{
		// Round is over, defense wins.
		ScoreAlternateWin((FRGS && FRGS->bRedToCap) ? 1 : 0, 1);
	}
	else if (RemainingTime - FRGS->EarlyEndTime <= 0)
	{
		// Round is over, defense wins.
		ScoreAlternateWin((FRGS && FRGS->bRedToCap) ? 1 : 0, 2);
	}
	else
	{
		if (FRGS)
		{
			uint8 OldBonusLevel = FRGS->BonusLevel;
			FRGS->BonusLevel = (RemainingTime >= FRGS->GoldBonusThreshold) ? 3 : 2;
			if (RemainingTime < FRGS->SilverBonusThreshold)
			{
				FRGS->BonusLevel = 1;
			}
			if (OldBonusLevel != FRGS->BonusLevel)
			{
				FRGS->OnBonusLevelChanged();
				FRGS->ForceNetUpdate();
			}
		}
	}
}

void AUTFlagRunGame::InitDelayedFlag(AUTCarriedObject* Flag)
{
	Super::InitDelayedFlag(Flag);
	if (Flag && IsTeamOnOffense(Flag->GetTeamNum()))
	{
		Flag->SetActorHiddenInGame(true);
		FFlagTrailPos NewPosition;
		NewPosition.Location = Flag->GetHomeLocation();
		NewPosition.MidPoints[0] = FVector(0.f);
		Flag->PutGhostFlagAt(NewPosition);
	}
}

void AUTFlagRunGame::InitFlagForRound(AUTCarriedObject* Flag)
{
	if (Flag != nullptr)
	{
		Flag->AutoReturnTime = 8.f;
		Flag->bGradualAutoReturn = true;
		Flag->bDisplayHolderTrail = true;
		Flag->bShouldPingFlag = true;
		Flag->bSlowsMovement = bSlowFlagCarrier;
		Flag->ClearGhostFlags();
		Flag->bSendHomeOnScore = false;
		if (IsTeamOnOffense(Flag->GetTeamNum()))
		{
			Flag->MessageClass = UUTFlagRunGameMessage::StaticClass();
			Flag->SetActorHiddenInGame(false);
			Flag->bEnemyCanPickup = false;
			Flag->bFriendlyCanPickup = true;
			Flag->bTeamPickupSendsHome = false;
			Flag->bEnemyPickupSendsHome = false;
			Flag->bWaitingForFirstPickup = true;
			GetWorldTimerManager().SetTimer(Flag->NeedFlagAnnouncementTimer, Flag, &AUTCarriedObject::SendNeedFlagAnnouncement, 5.f, false);
			ActiveFlag = Flag;
		}
		else
		{
			Flag->Destroy();
		}
	}
}

void AUTFlagRunGame::NotifyFirstPickup(AUTCarriedObject* Flag)
{
	if (Flag && Flag->HoldingPawn && StartingArmorClass)
	{
		if (!StartingArmorClass.GetDefaultObject()->HandleGivenTo(Flag->HoldingPawn))
		{
			Flag->HoldingPawn->AddInventory(GetWorld()->SpawnActor<AUTInventory>(StartingArmorClass, FVector(0.0f), FRotator(0.f, 0.f, 0.f)), true);
		}
	}
}

void AUTFlagRunGame::IntermissionSwapSides()
{
	// swap sides, if desired
	AUTWorldSettings* Settings = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
	if (Settings != NULL && Settings->bAllowSideSwitching)
	{
		CTFGameState->ChangeTeamSides(1);
	}
	else
	{
		// force update of flags since defender flag gets destroyed
		for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
		{
			IUTTeamInterface* TeamObj = Cast<IUTTeamInterface>(Base);
			if (TeamObj != NULL)
			{
				TeamObj->Execute_SetTeamForSideSwap(Base, Base->TeamNum);
			}
		}
	}
}

void AUTFlagRunGame::InitFlags()
{
	Super::InitFlags();
	for (AUTCTFFlagBase* Base : CTFGameState->FlagBases)
	{
		if (Base)
		{
			Base->Capsule->SetCapsuleSize(160.f, 134.0f);
		}
	}
}

int32 AUTFlagRunGame::PickCheatWinTeam()
{
	AUTFlagRunGameState* FRGS = Cast<AUTFlagRunGameState>(CTFGameState);
	return (FRGS && FRGS->bRedToCap) ? 0 : 1;
}

bool AUTFlagRunGame::CheckForWinner(AUTTeamInfo* ScoringTeam)
{
	if (Super::CheckForWinner(ScoringTeam))
	{
		return true;
	}

	// Check if a team has an insurmountable lead
	// current implementation assumes 6 rounds and 2 teams
	if (CTFGameState && (CTFGameState->CTFRound >= NumRounds - 2) && Teams[0] && Teams[1])
	{
		AUTFlagRunGameState* GS = Cast<AUTFlagRunGameState>(CTFGameState);
		bSecondaryWin = false;
		if ((CTFGameState->CTFRound == NumRounds - 2) && (FMath::Abs(Teams[0]->Score - Teams[1]->Score) > DefenseScore + GoldScore))
		{
			AUTTeamInfo* BestTeam = (Teams[0]->Score > Teams[1]->Score) ? Teams[0] : Teams[1];
			EndTeamGame(BestTeam, FName(TEXT("scorelimit")));
			return true;
		}
		if (CTFGameState->CTFRound == NumRounds - 1)
		{
			if (GS && GS->bRedToCap)
			{
				// next round is blue cap
				if ((Teams[0]->Score > Teams[1]->Score + GoldScore) || ((GS->TiebreakValue > 60) && (Teams[0]->Score == Teams[1]->Score + GoldScore)))
				{
					EndTeamGame(Teams[0], FName(TEXT("scorelimit")));
					return true;
				}
				if ((Teams[1]->Score > Teams[0]->Score + DefenseScore) || ((Teams[1]->Score == Teams[0]->Score + DefenseScore) && (GS->TiebreakValue < 0)))
				{
					EndTeamGame(Teams[1], FName(TEXT("scorelimit")));
					return true;
				}
			}
			else
			{
				// next round is red cap
				if ((Teams[1]->Score > Teams[0]->Score + GoldScore) || ((GS->TiebreakValue < -60) && (Teams[1]->Score == Teams[0]->Score + GoldScore)))
				{
					EndTeamGame(Teams[1], FName(TEXT("scorelimit")));
					return true;
				}
				if ((Teams[0]->Score > Teams[1]->Score + DefenseScore) || ((Teams[0]->Score == Teams[1]->Score + DefenseScore) && (GS->TiebreakValue > 0)))
				{
					EndTeamGame(Teams[0], FName(TEXT("scorelimit")));
					return true;
				}
			}
		}
	}
	return false;
}

void AUTFlagRunGame::DefaultTimer()
{
	Super::DefaultTimer();

	if (UTGameState && UTGameState->IsMatchInProgress() && !UTGameState->IsMatchIntermission())
	{
		AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);
		if (RCTFGameState && (RCTFGameState->RemainingPickupDelay <= 0) && (GetWorld()->GetTimeSeconds() - LastEntryDefenseWarningTime > 15.f))
		{
			// check for uncovered routes - support up to 5 entries for now
			AUTGameVolume* EntryRoutes[MAXENTRYROUTES];
			for (int32 i = 0; i < MAXENTRYROUTES; i++)
			{
				EntryRoutes[i] = nullptr;
			}
			// mark routes that need to be covered
			for (TActorIterator<AUTGameVolume> It(GetWorld()); It; ++It)
			{
				AUTGameVolume* GV = *It;
				if ((GV->RouteID > 0) && (GV->RouteID < MAXENTRYROUTES) && GV->bReportDefenseStatus && (GV->VoiceLinesSet != NAME_None))
				{
					EntryRoutes[GV->RouteID] = GV;
				}
			}
			// figure out where defenders are
			bool bFoundInnerDefender = false;
			AUTPlayerState* Speaker = nullptr;
			FString Why = "";
			for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
			{
				AUTCharacter* UTChar = Cast<AUTCharacter>((*Iterator)->GetPawn());
				AUTPlayerState* UTPS = Cast<AUTPlayerState>((*Iterator)->PlayerState);
				if (UTChar && UTChar->LastGameVolume && UTPS && UTPS->Team && IsTeamOnDefense(UTPS->Team->TeamIndex))
				{
					Speaker = UTPS;
					int32 CoveredRoute = UTChar->LastGameVolume->RouteID;
					if ((CoveredRoute == 0) || UTChar->LastGameVolume->bIsTeamSafeVolume)
					{
						bFoundInnerDefender = true;
						break;
					}
					else if ((CoveredRoute > 0) && (CoveredRoute < MAXENTRYROUTES))
					{
						EntryRoutes[CoveredRoute] = nullptr;
					}
					else
					{
						//						UE_LOG(UT, Warning, TEXT("Not in defensive position %s %s routeid %d"), *UTChar->LastGameVolume->GetName(), *UTChar->LastGameVolume->VolumeName.ToString(), UTChar->LastGameVolume->RouteID);
					}
					//Why = Why + FString::Printf(TEXT("%s in position %s routeid %d, "), *UTPS->PlayerName, *UTChar->LastGameVolume->VolumeName.ToString(), UTChar->LastGameVolume->RouteID);
				}
			}
			if (!bFoundInnerDefender && Speaker)
			{
				// warn about any uncovered entries
				for (int32 i = 0; i < MAXENTRYROUTES; i++)
				{
					if (EntryRoutes[i] && (EntryRoutes[i]->VoiceLinesSet != NAME_None))
					{
						LastEntryDefenseWarningTime = GetWorld()->GetTimeSeconds();
						/*
						if (Cast<AUTPlayerController>(Speaker->GetOwner()))
						{
							Cast<AUTPlayerController>(Speaker->GetOwner())->TeamSay(Why);
						}
						else if (Cast<AUTBot>(Speaker->GetOwner()))
						{
							Cast<AUTBot>(Speaker->GetOwner())->Say(Why, true);
						}
						*/
						Speaker->AnnounceStatus(EntryRoutes[i]->VoiceLinesSet, 3);
					}
				}
			}
		}
	}
}

float AUTFlagRunGame::OverrideRespawnTime(AUTPickupInventory* Pickup, TSubclassOf<AUTInventory> InventoryType)
{
	if (!Pickup || !InventoryType)
	{
		return 0.f;
	}
	AUTWeapon* WeaponDefault = Cast<AUTWeapon>(InventoryType.GetDefaultObject());
	if (WeaponDefault)
	{
		if (WeaponDefault->bMustBeHolstered)
		{
			Pickup->bSpawnOncePerRound = true;
			int32 RoundTime = (TimeLimit == 0) ? 300 : TimeLimit;
			return FMath::Max(20.f, RoundTime - 120.f);
		}
		return 20.f;
	}
	return InventoryType.GetDefaultObject()->RespawnTime;
}

int32 AUTFlagRunGame::GetComSwitch(FName CommandTag, AActor* ContextActor, AUTPlayerController* InInstigator, UWorld* World)
{
	if (World == nullptr) return INDEX_NONE;

	AUTFlagRunGameState* GS = Cast<AUTFlagRunGameState>(CTFGameState);

	if (Instigator == nullptr || GS == nullptr)
	{
		return Super::GetComSwitch(CommandTag, ContextActor, InInstigator, World);
	}

	AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(InInstigator->PlayerState);
	AUTCharacter* ContextCharacter = ContextActor != nullptr ? Cast<AUTCharacter>(ContextActor) : nullptr;
	AUTPlayerState* ContextPlayerState = ContextCharacter != nullptr ? Cast<AUTPlayerState>(ContextCharacter->PlayerState) : nullptr;
	
	uint8 OffensiveTeamNum = GS->bRedToCap ? 0 : 1;

	if (ContextCharacter)
	{
		bool bContextOnSameTeam = ContextCharacter != nullptr ? World->GetGameState<AUTGameState>()->OnSameTeam(InInstigator, ContextCharacter) : false;
		bool bContextIsFlagCarrier = ContextPlayerState != nullptr && ContextPlayerState->CarriedObject != nullptr;

		if (bContextIsFlagCarrier)
		{
			if ( bContextOnSameTeam )
			{
				if ( CommandTag == CommandTags::Intent )
				{
					return GOT_YOUR_BACK_SWITCH_INDEX;
				}

				else if (CommandTag == CommandTags::Attack)
				{
					return GOING_IN_SWITCH_INDEX;
				}

				else if (CommandTag == CommandTags::Defend)
				{
					return ATTACK_THEIR_BASE_SWITCH_INDEX;
				}
			}
			else
			{
				if (CommandTag == CommandTags::Intent)
				{
					return ENEMY_FC_HERE_SWITCH_INDEX;
				}
				else if (CommandTag == CommandTags::Attack)
				{
					return GET_FLAG_BACK_SWITCH_INDEX;
				}
				else if (CommandTag == CommandTags::Defend)
				{
					return BASE_UNDER_ATTACK_SWITCH_INDEX;
				}
			}
		}
	}

	AUTCharacter* InstCharacter = Cast<AUTCharacter>(InInstigator->GetCharacter());
	if (InstCharacter != nullptr && !InstCharacter->IsDead())
	{
		// We aren't dead, look to see if we have the flag...
			
		if (UTPlayerState->CarriedObject != nullptr)
		{
			if (CommandTag == CommandTags::Intent)			
			{
				return GOT_FLAG_SWITCH_INDEX;
			}
			if (CommandTag == CommandTags::Attack)			
			{
				return ATTACK_THEIR_BASE_SWITCH_INDEX;
			}
			if (CommandTag == CommandTags::Defend)			
			{
				return DEFEND_FLAG_CARRIER_SWITCH_INDEX;
			}
		}
	}

	if (CommandTag == CommandTags::Intent)
	{
		// Look to see if I'm on offense or defense...

		if (InInstigator->GetTeamNum() == OffensiveTeamNum)
		{
			return ATTACK_THEIR_BASE_SWITCH_INDEX;
		}
		else
		{
			return AREA_SECURE_SWITCH_INDEX;
		}
	}

	if (CommandTag == CommandTags::Attack)
	{
		// Look to see if I'm on offense or defense...

		if (InInstigator->GetTeamNum() == OffensiveTeamNum)
		{
			return ATTACK_THEIR_BASE_SWITCH_INDEX;
		}
		else
		{
			return ON_OFFENSE_SWITCH_INDEX;
		}
	}

	if (CommandTag == CommandTags::Defend)
	{
		// Look to see if I'm on offense or defense...

		if (InInstigator->GetTeamNum() == OffensiveTeamNum)
		{
			return ON_DEFENSE_SWITCH_INDEX;
		}
		else
		{
			return SPREAD_OUT_SWITCH_INDEX;
		}
	}

	if (CommandTag == CommandTags::Distress)
	{
		return UNDER_HEAVY_ATTACK_SWITCH_INDEX;  
	}

	return Super::GetComSwitch(CommandTag, ContextActor, InInstigator, World);
}

bool AUTFlagRunGame::HandleRallyRequest(AController* C)
{
	if (C == nullptr)
	{
		return false;
	}
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(C->GetPawn());
	AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(C->PlayerState);

	// if can rally, teleport with transloc effect, set last rally time
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	AUTTeamInfo* Team = UTPlayerState ? UTPlayerState->Team : nullptr;
	if (Team && !UTCharacter->GetCarriedObject() && GS->CurrentRallyPoint && UTPlayerState->bCanRally && !GetWorldTimerManager().IsTimerActive(UTPlayerState->RallyTimerHandle) && GS->bAttackersCanRally && UTCharacter && GS && IsMatchInProgress() && !GS->IsMatchIntermission() && ((Team->TeamIndex == 0) == GS->bRedToCap) && GS->FlagBases.IsValidIndex(Team->TeamIndex) && GS->FlagBases[Team->TeamIndex] != nullptr)
	{
		UTPlayerState->RallyLocation = GS->CurrentRallyPoint->GetRallyLocation(UTCharacter);
		UTPlayerState->RallyPoint = GS->CurrentRallyPoint;
		UTCharacter->bTriggerRallyEffect = true;
		UTCharacter->OnTriggerRallyEffect();
		UTPlayerState->BeginRallyTo(UTPlayerState->RallyPoint, UTPlayerState->RallyLocation, 1.2f);
		UTCharacter->SpawnRallyDestinationEffectAt(UTPlayerState->RallyLocation);
		if (UTCharacter->UTCharacterMovement)
		{
			UTCharacter->UTCharacterMovement->StopMovementImmediately();
			UTCharacter->UTCharacterMovement->DisableMovement();
			UTCharacter->DisallowWeaponFiring(true);
		}
		return true;
	}
	return false;
}

void AUTFlagRunGame::CompleteRallyRequest(AController* C)
{
	if (C == nullptr)
	{
		return;
	}
	AUTPlayerController* RequestingPC = Cast<AUTPlayerController>(C);
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(C->GetPawn());
	AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(C->PlayerState);

	// if can rally, teleport with transloc effect, set last rally time
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	AUTTeamInfo* Team = UTPlayerState ? UTPlayerState->Team : nullptr;
	if (!UTCharacter || !IsMatchInProgress() || !GS || GS->IsMatchIntermission() || UTCharacter->IsPendingKillPending())
	{
		return;
	}
	UTCharacter->bTriggerRallyEffect = false;
	if (UTCharacter->UTCharacterMovement)
	{
		UTCharacter->UTCharacterMovement->SetDefaultMovementMode();
	}
	UTCharacter->DisallowWeaponFiring(false);
	if (!UTCharacter->bCanRally)
	{
		if (RequestingPC != nullptr)
		{
			RequestingPC->ClientPlaySound(RallyFailedSound);
		}
		return;
	}

	if (Team && ((Team->TeamIndex == 0) == GS->bRedToCap) && GS->FlagBases.IsValidIndex(Team->TeamIndex) && GS->FlagBases[Team->TeamIndex] != nullptr)
	{
		FVector WarpLocation = FVector::ZeroVector;
		FRotator WarpRotation = UTPlayerState->RallyPoint ? UTPlayerState->RallyPoint->GetActorRotation() : UTCharacter->GetActorRotation();
		WarpRotation.Pitch = 0.f;
		WarpRotation.Roll = 0.f;
		ECollisionChannel SavedObjectType = UTCharacter->GetCapsuleComponent()->GetCollisionObjectType();
		UTCharacter->GetCapsuleComponent()->SetCollisionObjectType(COLLISION_TELEPORTING_OBJECT);

		if (GetWorld()->FindTeleportSpot(UTCharacter, UTPlayerState->RallyLocation, WarpRotation))
		{
			WarpLocation = UTPlayerState->RallyLocation;
		}
		else if (UTPlayerState->RallyPoint)
		{
			WarpLocation = UTPlayerState->RallyPoint->GetRallyLocation(UTCharacter);
		}
		else
		{
			if (RequestingPC != nullptr)
			{
				RequestingPC->ClientPlaySound(RallyFailedSound);
			}
			return;
		}
		UTCharacter->GetCapsuleComponent()->SetCollisionObjectType(SavedObjectType);
		float RallyDelay = 15.f;

		// teleport
		UPrimitiveComponent* SavedPlayerBase = UTCharacter->GetMovementBase();
		FTransform SavedPlayerTransform = UTCharacter->GetTransform();
		if (UTCharacter->TeleportTo(WarpLocation, WarpRotation))
		{
			if (RequestingPC != nullptr)
			{
				RequestingPC->UTClientSetRotation(WarpRotation);
			}
			UTPlayerState->NextRallyTime = GetWorld()->GetTimeSeconds() + RallyDelay;

			if (TranslocatorClass)
			{
				if (TranslocatorClass->GetDefaultObject<AUTWeap_Translocator>()->DestinationEffect != nullptr)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Instigator = UTCharacter;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.Owner = UTCharacter;
					GetWorld()->SpawnActor<AUTReplicatedEmitter>(TranslocatorClass->GetDefaultObject<AUTWeap_Translocator>()->DestinationEffect, UTCharacter->GetActorLocation(), UTCharacter->GetActorRotation(), SpawnParams);
				}
				UUTGameplayStatics::UTPlaySound(GetWorld(), TranslocatorClass->GetDefaultObject<AUTWeap_Translocator>()->TeleSound, UTCharacter, SRT_All);
			}

			// spawn effects
			FActorSpawnParameters SpawnParams;
			SpawnParams.Instigator = UTCharacter;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.Owner = UTCharacter;
			if (AfterImageType != NULL)
			{
				AUTWeaponRedirector* AfterImage = GetWorld()->SpawnActor<AUTWeaponRedirector>(AfterImageType, SavedPlayerTransform.GetLocation(), SavedPlayerTransform.GetRotation().Rotator(), SpawnParams);
				if (AfterImage != NULL)
				{
					float HalfHeight = UTCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
					float Radius = UTCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
					FCollisionShape PlayerCapsule = FCollisionShape::MakeCapsule(Radius, HalfHeight);
					AfterImage->InitFor(UTCharacter, FRepCollisionShape(PlayerCapsule), SavedPlayerBase, UTCharacter->GetTransform());
				}
			}

			// announce
			AActor* RallySpot = UTPlayerState->RallyPoint ? UTPlayerState->RallyPoint->MyGameVolume : nullptr;
			if (RallySpot == nullptr)
			{
				UTCharacter->UTCharacterMovement->UpdatedComponent->UpdatePhysicsVolume(true);
				RallySpot = UTCharacter->UTCharacterMovement ? UTCharacter->UTCharacterMovement->GetPhysicsVolume() : nullptr;
				if ((RallySpot == nullptr) || (RallySpot == GetWorld()->GetDefaultPhysicsVolume()))
				{
					AUTCTFFlag* CarriedFlag = Cast<AUTCTFFlag>(GS->FlagBases[GS->bRedToCap ? 0 : 1]->GetCarriedObject());
					if (CarriedFlag)
					{
						RallySpot = CarriedFlag;
					}
				}
			}
			if (UTPlayerState->RallyPoint && GS->CurrentRallyPoint == UTPlayerState->RallyPoint)
			{
				GS->bEnemyRallyPointIdentified = true;
			}
			UTPlayerState->ModifyStatsValue(NAME_Rallies, 1);
			
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
				if (PC)
				{
					if (GS->OnSameTeam(UTPlayerState, PC))
					{
						PC->ClientReceiveLocalizedMessage(UUTCTFMajorMessage::StaticClass(), 27, UTPlayerState);
						if (GetWorld()->GetTimeSeconds() - RallyRequestTime < 6.f)
						{
							PC->ClientReceiveLocalizedMessage(UTPlayerState->GetCharacterVoiceClass(), ACKNOWLEDGE_SWITCH_INDEX, UTPlayerState, PC->PlayerState, NULL);
						}
					}
					else
					{
						PC->ClientReceiveLocalizedMessage(UUTCTFMajorMessage::StaticClass(), 24, UTPlayerState, nullptr, RallySpot); 
					}
				}
			}
		}
	}
}

void AUTFlagRunGame::HandleMatchIntermission()
{
	Super::HandleMatchIntermission();
	AUTFlagRunGameState* GS = Cast<AUTFlagRunGameState>(UTGameState);
	if (GS && GS->GetScoringPlays().Num() > 0)
	{
		GS->UpdateMatchHighlights();
	}
	if ((GS == nullptr) || (GS->CTFRound < GS->NumRounds - 2))
	{
		return;
	}

	// Update win requirements if last two rounds
	AUTTeamInfo* NextAttacker = (GS->bRedToCap == GS->IsMatchIntermission()) ? GS->Teams[1] : GS->Teams[0];
	AUTTeamInfo* NextDefender = (GS->bRedToCap == GS->IsMatchIntermission()) ? GS->Teams[0] : GS->Teams[1];
	int32 RequiredTime = (GS->bRedToCap == GS->IsMatchIntermission()) ? GS->TiebreakValue : -1 * GS->TiebreakValue;
	RequiredTime = FMath::Max(RequiredTime, 0);
	GS->FlagRunMessageTeam = nullptr;
	GS->EarlyEndTime = 0;
	if (GS->CTFRound == GS->NumRounds - 2)
	{
		if (NextAttacker->Score > NextDefender->Score)
		{
			GS->FlagRunMessageTeam = NextDefender;
			if (NextAttacker->Score - NextDefender->Score > 2)
			{
				// Defenders must stop attackers to have a chance
				GS->FlagRunMessageSwitch = 1;
			}
			else
			{
				int32 BonusType = (NextAttacker->Score - NextDefender->Score == 2) ? 1 : 2;
				GS->FlagRunMessageSwitch = BonusType + 1;
			}
		}
		else if (NextDefender->Score > NextAttacker->Score)
		{
			GS->FlagRunMessageTeam = NextAttacker;
			
			int32 BonusType = FMath::Max(1, (NextDefender->Score - NextAttacker->Score) - 1);
			if (RequiredTime > 60)
			{
				BonusType++;
				RequiredTime = 0;
			}
			BonusType = FMath::Min(BonusType, 3);
			GS->FlagRunMessageSwitch = 100 * RequiredTime + BonusType + 3;
			GS->EarlyEndTime = 60 * (BonusType - 1) + RequiredTime;
		}
	}
	else if (GS->CTFRound == GS->NumRounds - 1)
	{
		bool bNeedTimeThreshold = false;
		GS->FlagRunMessageTeam = NextAttacker;
		if (NextDefender->Score <= NextAttacker->Score)
		{
			GS->FlagRunMessageSwitch = 8;
		}
		else
		{
			int32 BonusType = NextDefender->Score - NextAttacker->Score;
			if (RequiredTime > 60)
			{
				BonusType++;
				RequiredTime = 0;
			}
			GS->FlagRunMessageSwitch = 7 + BonusType + 100 * RequiredTime;
			GS->EarlyEndTime = 60 * (BonusType - 1) + RequiredTime;
		}
	}
}


void AUTFlagRunGame::CheatScore()
{
	if ((UE_BUILD_DEVELOPMENT || (GetNetMode() == NM_Standalone)) && !bOfflineChallenge && !bBasicTrainingGame && UTGameState)
	{
		UTGameState->SetRemainingTime(FMath::RandHelper(150));
		IntermissionDuration = 12.f;
	}
	Super::CheatScore();
}

void AUTFlagRunGame::UpdateSkillRating()
{
	if (bRankedSession)
	{
		ReportRankedMatchResults(GetRankedLeagueName());
	}
	else
	{
		ReportRankedMatchResults(NAME_FlagRunSkillRating.ToString());
	}
}

FString AUTFlagRunGame::GetRankedLeagueName()
{
	return NAME_RankedFlagRunSkillRating.ToString();
}

uint8 AUTFlagRunGame::GetNumMatchesFor(AUTPlayerState* PS, bool bInRankedSession) const
{
	return PS ? PS->FlagRunMatchesPlayed : 0;
}

int32 AUTFlagRunGame::GetEloFor(AUTPlayerState* PS, bool bInRankedSession) const
{
	return PS ? PS->FlagRunRank : Super::GetEloFor(PS, bInRankedSession);
}

void AUTFlagRunGame::SetEloFor(AUTPlayerState* PS, bool bInRankedSession, int32 NewEloValue, bool bIncrementMatchCount)
{
	if (PS)
	{
		if (bInRankedSession)
		{
			PS->RankedFlagRunRank = NewEloValue;
			if (bIncrementMatchCount && (PS->ShowdownMatchesPlayed < 255))
			{
				PS->RankedFlagRunMatchesPlayed++;
			}

		}
		else
		{
			PS->FlagRunRank = NewEloValue;
			if (bIncrementMatchCount && (PS->FlagRunMatchesPlayed < 255))
			{
				PS->FlagRunMatchesPlayed++;
			}
		}
	}
}

void AUTFlagRunGame::GrantPowerupToTeam(int TeamIndex, AUTPlayerState* PlayerToHighlight)
{
	if (!bAllowBoosts)
	{
		return;
	}
	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS && PS->Team)
		{
			if (PS->Team->TeamIndex == TeamIndex)
			{
				if (PS->BoostClass && PS->BoostClass.GetDefaultObject() && PS->BoostClass.GetDefaultObject()->RemainingBoostsGivenOverride > 0)
				{
					PS->SetRemainingBoosts(PS->BoostClass.GetDefaultObject()->RemainingBoostsGivenOverride);
				}
				else
				{
					PS->SetRemainingBoosts(1);
				}
			}
			AUTPlayerController* PC = Cast<AUTPlayerController>(PS->GetOwner());
			if (PC)
			{
				if (PS->Team->TeamIndex == TeamIndex)
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFRewardMessage::StaticClass(), 7, PlayerToHighlight);
				}
				else
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), 7, PlayerToHighlight);
				}
			}
		}
	}
}

void AUTFlagRunGame::HandleTeamChange(AUTPlayerState* PS, AUTTeamInfo* OldTeam)
{
	// If a player doesn't have a valid selected boost powerup, lets go ahead and give them the 1st one available in the Powerup List
	if (PS && UTGameState && bAllowBoosts)
	{
		if (!PS->BoostClass || !UTGameState->IsSelectedBoostValid(PS))
		{
			TSubclassOf<class AUTInventory> SelectedBoost = UTGameState->GetSelectableBoostByIndex(PS, 0);
			PS->BoostClass = SelectedBoost;
		}
	}
	Super::HandleTeamChange(PS, OldTeam);
}

void AUTFlagRunGame::ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	Super::ScoreKill_Implementation(Killer, Other, KilledPawn, DamageType);
	HandlePowerupUnlocks(KilledPawn, Killer);
}

void AUTFlagRunGame::HandlePowerupUnlocks(APawn* Other, AController* Killer)
{
	AUTPlayerState* KillerPS = Killer ? Cast<AUTPlayerState>(Killer->PlayerState) : nullptr;
	AUTPlayerState* VictimPS = Other ? Cast<AUTPlayerState>(Other->PlayerState) : nullptr;

	UpdatePowerupUnlockProgress(VictimPS, KillerPS);

	const int RedTeamIndex = 0;
	const int BlueTeamIndex = 1;

	AUTFlagRunGameState* RCTFGameState = Cast<AUTFlagRunGameState>(CTFGameState);
	if (RCTFGameState)
	{
		if ((RCTFGameState->OffenseKills >= OffenseKillsNeededForPowerUp) && RCTFGameState->bIsOffenseAbleToGainPowerup)
		{
			RCTFGameState->OffenseKills = 0;

			GrantPowerupToTeam(IsTeamOnOffense(RedTeamIndex) ? RedTeamIndex : BlueTeamIndex, KillerPS);
			RCTFGameState->bIsOffenseAbleToGainPowerup = false;
		}

		if ((RCTFGameState->DefenseKills >= DefenseKillsNeededForPowerUp) && RCTFGameState->bIsDefenseAbleToGainPowerup)
		{
			RCTFGameState->DefenseKills = 0;

			GrantPowerupToTeam(IsTeamOnDefense(RedTeamIndex) ? RedTeamIndex : BlueTeamIndex, KillerPS);
			RCTFGameState->bIsDefenseAbleToGainPowerup = false;
		}
	}
}

void AUTFlagRunGame::UpdatePowerupUnlockProgress(AUTPlayerState* VictimPS, AUTPlayerState* KillerPS)
{
	AUTCTFRoundGameState* RCTFGameState = Cast<AUTCTFRoundGameState>(CTFGameState);

	if (RCTFGameState && VictimPS && VictimPS->Team && KillerPS && KillerPS->Team)
	{
		//No credit for suicides
		if (VictimPS->Team->TeamIndex != KillerPS->Team->TeamIndex)
		{
			if (IsTeamOnDefense(VictimPS->Team->TeamIndex))
			{
				++(RCTFGameState->OffenseKills);
			}
			else
			{
				++(RCTFGameState->DefenseKills);
			}
		}
	}
}

AActor* AUTFlagRunGame::SetIntermissionCameras(uint32 TeamToWatch)
{
	AUTFlagRunGameState* FRGS = Cast<AUTFlagRunGameState>(CTFGameState);
	if (FRGS)
	{
		return FRGS->FlagBases[(FRGS && FRGS->bRedToCap) ? 1 : 0];
	}
	return nullptr;
}

bool AUTFlagRunGame::IsTeamOnOffense(int32 TeamNumber) const
{
	AUTFlagRunGameState* FRGS = Cast<AUTFlagRunGameState>(CTFGameState);
	return FRGS && (FRGS->bRedToCap == (TeamNumber == 0));
}

void AUTFlagRunGame::SendRestartNotifications(AUTPlayerState* PS, AUTPlayerController* PC)
{
	if (PS->Team && IsTeamOnOffense(PS->Team->TeamIndex))
	{
		LastAttackerSpawnTime = GetWorld()->GetTimeSeconds();
	}
	if (PC && (PS->GetRemainingBoosts() > 0))
	{
		PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), 20);
	}
}

void AUTFlagRunGame::ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason)
{
	Super::ScoreObject_Implementation(GameObject,HolderPawn,Holder,Reason);

	if (FUTAnalytics::IsAvailable())
	{
		FUTAnalytics::FireEvent_FlagRunRoundEnd(this, false, (UTGameState->WinningTeam != nullptr));
	}

	if (UTGameState)
	{
		UTGameState->ScoringPlayerState = Cast<AUTPlayerState>(HolderPawn->PlayerState);
	}
}

void AUTFlagRunGame::ScoreAlternateWin(int32 WinningTeamIndex, uint8 Reason /* = 0 */)
{
	Super::ScoreAlternateWin(WinningTeamIndex, Reason);

	if (FUTAnalytics::IsAvailable())
	{
		FUTAnalytics::FireEvent_FlagRunRoundEnd(this, IsTeamOnDefense(WinningTeamIndex), (UTGameState->WinningTeam != nullptr));
	}
}

bool AUTFlagRunGame::SupportsInstantReplay() const
{
	return true;
}

void AUTFlagRunGame::FindAndMarkHighScorer()
{
	for (int32 i = 0; i < Teams.Num(); i++)
	{
		int32 BestScore = 0;

		for (int32 PlayerIdx = 0; PlayerIdx < Teams[i]->GetTeamMembers().Num(); PlayerIdx++)
		{
			if (Teams[i]->GetTeamMembers()[PlayerIdx] != nullptr)
			{
				AUTPlayerState *PS = Cast<AUTPlayerState>(Teams[i]->GetTeamMembers()[PlayerIdx]->PlayerState);
				if (PS != nullptr)
				{
					PS->Score = PS->RoundKillAssists + PS->RoundKills;
					if (BestScore == 0 || PS->Score > BestScore)
					{
						BestScore = PS->Score;
					}
				}
			}
		}

		for (int32 PlayerIdx = 0; PlayerIdx < Teams[i]->GetTeamMembers().Num(); PlayerIdx++)
		{
			if (Teams[i]->GetTeamMembers()[PlayerIdx] != nullptr)
			{
				AUTPlayerState *PS = Cast<AUTPlayerState>(Teams[i]->GetTeamMembers()[PlayerIdx]->PlayerState);
				if (PS != nullptr)
				{
					bool bOldHighScorer = PS->bHasHighScore;
					PS->bHasHighScore = (BestScore == PS->Score) && (BestScore > 0);
					if ((bOldHighScorer != PS->bHasHighScore) && (GetNetMode() != NM_DedicatedServer))
					{
						PS->OnRep_HasHighScore();
					}
				}
			}
		}
	}
}

void AUTFlagRunGame::HandleRollingAttackerRespawn(AUTPlayerState* OtherPS)
{
	Super::HandleRollingAttackerRespawn(OtherPS);
	AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
	int32 RoundTime = (TimeLimit == 0) ? 300 : TimeLimit;
	if (GS && !GS->bAttackersCanRally && (GetWorld()->GetTimeSeconds() > OtherPS->NextRallyTime) && GS->bHaveEstablishedFlagRunner && !GS->CurrentRallyPoint && (GS->GetRemainingTime() < RoundTime - 30))
	{
		OtherPS->AnnounceStatus(StatusMessage::NeedRally);
	}
	else if (GS && GS->CurrentRallyPoint && GS->bAttackersCanRally && (GS->CurrentRallyPoint->RallyTimeRemaining > FMath::Max(float(OtherPS->RemainingRallyDelay), 2.5f)))
	{
		float DesiredRespawnDelay = GS->CurrentRallyPoint->RallyTimeRemaining - 2.f;
		if (OtherPS->RespawnWaitTime > DesiredRespawnDelay)
		{
			OtherPS->RespawnWaitTime = FMath::Max(1.f, DesiredRespawnDelay);
		}
	}
}

void AUTFlagRunGame::PlayEndOfMatchMessage()
{
	// stub to not break the build
}