// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameMode.h"
#include "UTPlayerState.h"
#include "Engine/LocalPlayer.h"
#include "Net/UnrealNetwork.h"
#include "UTGameMessage.h"
#include "UTHat.h"
#include "UTCharacterContent.h"
#include "SUWindowsStyle.h"
#include "Dialogs/SUTPlayerInfoDialog.h"
#include "Widgets/SUTTabWidget.h"
#include "StatNames.h"
#include "UTAnalytics.h"
#include "UTReplicatedMapInfo.h"
#include "UTRewardMessage.h"
#include "UTGameEngine.h"
#include "UTFlagInfo.h"
#include "UTEngineMessage.h"
#include "UTGameState.h"
#include "UTDemoRecSpectator.h"
#include "SUTStyle.h"
#include "UTHUDWidget_SpectatorSlideOut.h"
#include "UTShowdownGame.h"
#include "UTDuelGame.h"
#include "UTDMGameMode.h"
#include "UTLobbyGameMode.h"
#include "UTTeamDMGameMode.h"
#include "UTCTFBaseGame.h"
#include "UTFlagRunGame.h"
#include "UTHUDWidget_NetInfo.h"
#include "UTMcpUtils.h"
#include "UTCTFRoundGame.h"
#include "UTFlagRunGameState.h"
#include "UTCTFMajorMessage.h"
#include "Engine/DemoNetDriver.h"
#include "UTDeathMessage.h"
#include "UTLineUpHelper.h"

/** disables load warnings for dedicated server where invalid client input can cause unpreventable logspam, but enables on clients so developers can make sure their stuff is working */
static inline ELoadFlags GetCosmeticLoadFlags()
{
	return IsRunningDedicatedServer() ? LOAD_NoWarn : LOAD_None;
}

AUTPlayerState::AUTPlayerState(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReadyToPlay = false;
	bIsWarmingUp = false;
	bPendingTeamSwitch = false;
	bCaster = false;
	LastKillTime = 0.0f;
	Kills = 0;
	KillAssists = 0;
	DamageDone = 0;
	RoundDamageDone = 0;
	RoundKills = 0;
	RoundKillAssists = 0;
	bOutOfLives = false;
	Deaths = 0;
	bShouldAutoTaunt = false;
	bSentLogoutAnalytics = false;
	NextRallyTime = 0.f;
	RemainingRallyDelay = 0;
	SelectionOrder = 255;

	// We want to be ticked.
	PrimaryActorTick.bCanEverTick = true;

	StatManager = nullptr;
	bReadStatsFromCloud = false;
	bSuccessfullyReadStatsFromCloud = false;
	bWroteStatsToCloud = false;
	bIsDemoRecording = false;
	EngineMessageClass = UUTEngineMessage::StaticClass();
	LastTauntTime = -1000.f;
	PrevXP = -1;
	TotalChallengeStars = 0;
	EmoteSpeed = 1.0f;
	bAnnounceWeaponSpree = false;
	bAnnounceWeaponReward = false;
	ReadyMode = 0;
	CurrentLoadoutPackTag = NAME_None;
	RespawnWaitTime = 0.f;

	CoolFactorCombinationWindow = 5.0f;
	CoolFactorBleedSpeed = 20.0f;
	MinimumConsiderationForCoolFactorHistory = 200.0f;
}

void AUTPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTPlayerState, CarriedObject);
	DOREPLIFETIME(AUTPlayerState, bReadyToPlay);
	DOREPLIFETIME(AUTPlayerState, bIsWarmingUp);
	DOREPLIFETIME(AUTPlayerState, bPendingTeamSwitch);
	DOREPLIFETIME(AUTPlayerState, RespawnWaitTime);
	DOREPLIFETIME(AUTPlayerState, bOutOfLives);
	DOREPLIFETIME(AUTPlayerState, RemainingLives);
	DOREPLIFETIME(AUTPlayerState, Kills);
	DOREPLIFETIME(AUTPlayerState, RoundKills);
	DOREPLIFETIME(AUTPlayerState, KillAssists);
	DOREPLIFETIME(AUTPlayerState, RoundKillAssists);
	DOREPLIFETIME(AUTPlayerState, Deaths);
	DOREPLIFETIME(AUTPlayerState, Team);
	DOREPLIFETIME(AUTPlayerState, FlagCaptures);
	DOREPLIFETIME(AUTPlayerState, FlagReturns);
	DOREPLIFETIME(AUTPlayerState, Assists);
	DOREPLIFETIME(AUTPlayerState, LastKillerPlayerState);
	DOREPLIFETIME(AUTPlayerState, ChatDestination);
	DOREPLIFETIME(AUTPlayerState, CountryFlag);
	DOREPLIFETIME(AUTPlayerState, Avatar);
	DOREPLIFETIME(AUTPlayerState, RemainingBoosts);
	DOREPLIFETIME(AUTPlayerState, BoostClass);
	DOREPLIFETIME(AUTPlayerState, BoostRechargePct);
	DOREPLIFETIME(AUTPlayerState, ShowdownRank);
	DOREPLIFETIME(AUTPlayerState, RankedShowdownRank);
	DOREPLIFETIME(AUTPlayerState, DuelRank);
	DOREPLIFETIME(AUTPlayerState, CTFRank);
	DOREPLIFETIME(AUTPlayerState, TDMRank);
	DOREPLIFETIME(AUTPlayerState, DMRank);
	DOREPLIFETIME(AUTPlayerState, TrainingLevel);
	DOREPLIFETIME(AUTPlayerState, PrevXP);
	DOREPLIFETIME(AUTPlayerState, TotalChallengeStars);
	DOREPLIFETIME(AUTPlayerState, SelectedCharacter);
	DOREPLIFETIME(AUTPlayerState, TauntClass);
	DOREPLIFETIME(AUTPlayerState, Taunt2Class);
	DOREPLIFETIME(AUTPlayerState, GroupTauntClass);
	DOREPLIFETIME(AUTPlayerState, ActiveGroupTaunt);
	DOREPLIFETIME(AUTPlayerState, bHasHighScore);
	DOREPLIFETIME(AUTPlayerState, HatClass);
	DOREPLIFETIME(AUTPlayerState, LeaderHatClass);
	DOREPLIFETIME(AUTPlayerState, EyewearClass);
	DOREPLIFETIME(AUTPlayerState, HatVariant);
	DOREPLIFETIME(AUTPlayerState, EyewearVariant);
	DOREPLIFETIME(AUTPlayerState, bSpecialPlayer);
	DOREPLIFETIME(AUTPlayerState, bSpecialTeamPlayer);
	DOREPLIFETIME(AUTPlayerState, bCanRally);
	DOREPLIFETIME(AUTPlayerState, OverrideHatClass);
	DOREPLIFETIME(AUTPlayerState, Loadout);
	DOREPLIFETIME(AUTPlayerState, KickCount);
	DOREPLIFETIME(AUTPlayerState, AvailableCurrency);
	DOREPLIFETIME(AUTPlayerState, StatsID);
	DOREPLIFETIME(AUTPlayerState, FavoriteWeapon);
	DOREPLIFETIME(AUTPlayerState, bIsRconAdmin);
	DOREPLIFETIME(AUTPlayerState, SelectionOrder);

	DOREPLIFETIME(AUTPlayerState, DuelMatchesPlayed);
	DOREPLIFETIME(AUTPlayerState, CTFMatchesPlayed);
	DOREPLIFETIME(AUTPlayerState, TDMMatchesPlayed);
	DOREPLIFETIME(AUTPlayerState, DMMatchesPlayed);
	DOREPLIFETIME(AUTPlayerState, ShowdownMatchesPlayed);
	DOREPLIFETIME(AUTPlayerState, FlagRunMatchesPlayed);
	DOREPLIFETIME(AUTPlayerState, RankedShowdownMatchesPlayed);

	DOREPLIFETIME(AUTPlayerState, bHasVoted);

	DOREPLIFETIME_CONDITION(AUTPlayerState, RespawnChoiceA, COND_None); // also used when replicating spawn choice to other players
	DOREPLIFETIME_CONDITION(AUTPlayerState, RespawnChoiceB, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, bChosePrimaryRespawnChoice, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, WeaponSpreeDamage, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, RemainingRallyDelay, COND_OwnerOnly);

	DOREPLIFETIME(AUTPlayerState, SpectatingID);
	DOREPLIFETIME(AUTPlayerState, SpectatingIDTeam);
	DOREPLIFETIME(AUTPlayerState, bCaster);
	DOREPLIFETIME(AUTPlayerState, bHasLifeLimit);
	DOREPLIFETIME_CONDITION(AUTPlayerState, bIsDemoRecording, COND_InitialOnly);
	DOREPLIFETIME(AUTPlayerState, MatchHighlights);
	DOREPLIFETIME(AUTPlayerState, MatchHighlightData);
	DOREPLIFETIME(AUTPlayerState, EmoteReplicationInfo);
	DOREPLIFETIME(AUTPlayerState, EmoteSpeed);
	DOREPLIFETIME_CONDITION(AUTPlayerState, WeaponSkins, COND_OwnerOnly);
	DOREPLIFETIME(AUTPlayerState, ReadyMode);

	DOREPLIFETIME(AUTPlayerState, CurrentLoadoutPackTag);

	DOREPLIFETIME_CONDITION(AUTPlayerState, PrimarySpawnInventory, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, SecondarySpawnInventory, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, AllowedLoadoutItemTags, COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(AUTPlayerState, CriticalObject, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, UnlockList, COND_OwnerOnly);

	// Allow "displayall UTPlayerState CurrentCoolFactor" in development builds
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DOREPLIFETIME_CONDITION(AUTPlayerState, CurrentCoolFactor, COND_OwnerOnly);
#endif
}

void AUTPlayerState::Destroyed()
{
	Super::Destroyed();
	GetWorldTimerManager().ClearAllTimersForObject(this);
}

void AUTPlayerState::IncrementDamageDone(int32 AddedDamage)
{
	DamageDone += AddedDamage;
	RoundDamageDone += AddedDamage;
	ThisLifeDamageDone += AddedDamage;
}

bool AUTPlayerState::IsFemale()
{
	return GetSelectedCharacter() && GetSelectedCharacter().GetDefaultObject()->bIsFemale;
}

void AUTPlayerState::SetPlayerName(const FString& S)
{
	PlayerName = S;

	// RepNotify callback won't get called by net code if we are the server
	ENetMode NetMode = GetNetMode();
	if (NetMode == NM_Standalone || NetMode == NM_ListenServer)
	{
		OnRep_PlayerName();
	}
	ForceNetUpdate();
	bHasValidClampedName = false;
}

void AUTPlayerState::OnRep_HasHighScore()
{
	AUTCharacter* UTChar = GetUTCharacter();
	if (UTChar)
	{
		UTChar->HasHighScoreChanged();
	}
}

void AUTPlayerState::UpdatePing(float InPing)
{
}

void AUTPlayerState::CalculatePing(float NewPing)
{
	if (NewPing < 0.f)
	{
		// caused by timestamp wrap around
		return;
	}

	float OldPing = ExactPing;
	Super::UpdatePing(NewPing);

	AUTPlayerController* PC = Cast<AUTPlayerController>(GetOwner());
	if (PC)
	{
		PC->LastPingCalcTime = GetWorld()->GetTimeSeconds();
		if (ExactPing != OldPing)
		{
			PC->ServerUpdatePing(ExactPing);
		}
		if (PC->bShowNetInfo && PC->NetInfoWidget)
		{
			PC->NetInfoWidget->AddPing(NewPing);
		}
	}
}

bool AUTPlayerState::ShouldAutoTaunt() const
{
	if (!bIsABot)
	{
		return bShouldAutoTaunt;
	}
	UUTGameUserSettings* GS = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	return GS != NULL && GS->GetBotSpeech() == BSO_All;
}

void AUTPlayerState::AnnounceKill()
{
	AUTBaseGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
	if (CharacterVoice && ShouldAutoTaunt() && GameMode)
	{
		int32 NumTaunts = CharacterVoice.GetDefaultObject()->TauntMessages.Num();
		if (NumTaunts > 0)
		{
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			int32 TauntSelectionIndex = GS ? GS->TauntSelectionIndex % NumTaunts : 0;
			int32 SelectedTaunt = TauntSelectionIndex + FMath::Min(FMath::RandRange(0, 2), NumTaunts - TauntSelectionIndex - 1);
			if (GS)
			{
				GS->TauntSelectionIndex += 3;
			}
			GameMode->BroadcastLocalized(GetOwner(), CharacterVoice, SelectedTaunt, this);
		}
	}
}

void AUTPlayerState::AnnounceSameTeam(AUTPlayerController* ShooterPC)
{
	if (CharacterVoice && ShouldAutoTaunt() && (GetWorld()->GetTimeSeconds() - ShooterPC->LastSameTeamTime > 5.f) 
		&& (CharacterVoice.GetDefaultObject()->SameTeamMessages.Num() > 0))
	{
		ShooterPC->LastSameTeamTime = GetWorld()->GetTimeSeconds();
		ShooterPC->ClientReceiveLocalizedMessage(CharacterVoice, CharacterVoice.GetDefaultObject()->SameTeamBaseIndex + FMath::RandRange(0, CharacterVoice.GetDefaultObject()->SameTeamMessages.Num() - 1), this, ShooterPC->PlayerState, NULL);
	}
}

void AUTPlayerState::AnnounceReactionTo(const AUTPlayerState* ReactionPS) const
{
	if (CharacterVoice && ShouldAutoTaunt())
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(ReactionPS->GetOwner());
		if (!PC)
		{
			return;
		}
		int32 NumFriendlyReactions = CharacterVoice.GetDefaultObject()->FriendlyReactions.Num();
		int32 NumEnemyReactions = CharacterVoice.GetDefaultObject()->EnemyReactions.Num();
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		bool bSameTeam = GS ? GS->OnSameTeam(this, ReactionPS) : false;
		if (GS == NULL || bSameTeam || (NumEnemyReactions == 0) || (FMath::FRand() < 0.6f))
		{
			// only friendly reaction if on same team
			if (NumFriendlyReactions > 0)
			{
				PC->ClientReceiveLocalizedMessage(CharacterVoice, CharacterVoice.GetDefaultObject()->FriendlyReactionBaseIndex + FMath::RandRange(0, CharacterVoice.GetDefaultObject()->FriendlyReactions.Num() - 1), const_cast<AUTPlayerState*>(this), PC->PlayerState, NULL);
				return;
			}
			else if (bSameTeam)
			{
				return;
			}
		}
		if (NumEnemyReactions > 0)
		{
			PC->ClientReceiveLocalizedMessage(CharacterVoice, CharacterVoice.GetDefaultObject()->EnemyReactionBaseIndex + FMath::RandRange(0, CharacterVoice.GetDefaultObject()->EnemyReactions.Num() - 1), const_cast<AUTPlayerState*>(this), PC->PlayerState, NULL);
		}
	}
}

void AUTPlayerState::AnnounceStatus(FName NewStatus, int32 SwitchOffset, bool bSkipSelf)
{
	GetCharacterVoiceClass();
	if (CharacterVoice != NULL)
	{
		// send to same team only
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (!GS)
		{
			return;
		}
		int32 Switch = CharacterVoice.GetDefaultObject()->GetStatusIndex(NewStatus);
		if (Switch < 0)
		{
			UE_LOG(UT, Warning, TEXT("No valid index found"));
			// no valid index found (NewStatus was not a valid selection)
			return;
		}
		Switch += SwitchOffset;
//		UE_LOG(UT, Warning, TEXT("Switch is %d"), Switch);
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
			if (PC && GS->OnSameTeam(this, PC) && (!bSkipSelf || (PC->PlayerState != this)))
			{
				PC->ClientReceiveLocalizedMessage(CharacterVoice, Switch, this, PC->PlayerState, NULL);
			}
		}
	}
}

bool AUTPlayerState::ShouldBroadCastWelcomeMessage(bool bExiting)
{
	return !bIsInactive && ((GetNetMode() == NM_Standalone) || (GetNetMode() == NM_Client));
}

void AUTPlayerState::NotifyTeamChanged_Implementation()
{
	AUTCharacter* P = GetUTCharacter();
	if (P != NULL && !P->bTearOff)
	{
		P->NotifyTeamChanged();
	}
	// HACK: remember last team player got on the URL for travelling purposes
	if (Team != NULL)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(GetOwner());
		if (PC != NULL)
		{
			UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
			if (LP != NULL)
			{
				LP->SetDefaultURLOption(TEXT("Team"), FString::FromInt(Team->TeamIndex));

				// make sure proper outlines are shown for new team
				AGameStateBase* GS = GetWorld()->GetGameState();
				if (GS != NULL)
				{
					for (int32 i = 0; i < GS->PlayerArray.Num(); i++)
					{
						AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
						if (PS != NULL && PS->bSpecialTeamPlayer && PS->GetUTCharacter() != NULL)
						{
							PS->UpdateSpecialTacComFor(PS->GetUTCharacter(), PC);
						}
					}
				}
			}
		}
	}
}

void AUTPlayerState::IncrementKillAssists(TSubclassOf<UDamageType> DamageType, bool bEnemyKill, AUTPlayerState* VictimPS)
{
	if (bEnemyKill)
	{
		// include sprees/weaponsprees/etc.?
		KillAssists++;
		RoundKillAssists++;
		ThisLifeKillAssists++;

		AUTPlayerController* PC = Cast<AUTPlayerController>(GetOwner());
		if (PC)
		{
			PC->ClientReceiveLocalizedMessage(UUTDeathMessage::StaticClass(), 2, this, VictimPS, nullptr);
		}

		CheckForMultiKill();
		LastKillTime = GetWorld()->TimeSeconds;

		TSubclassOf<UUTDamageType> UTDamage(*DamageType);
		int32 SpreeIndex = IncrementWeaponSpree(UTDamage);
		if (SpreeIndex >= 0)
		{
			if (WeaponSprees[SpreeIndex].Kills == UTDamage.GetDefaultObject()->WeaponSpreeCount)
			{
				// only announce weapon spree on finishing kill with a weapon
				WeaponSprees[SpreeIndex].Kills--;
			}
			// more likely to kill again with same weapon, so shorten search through array by swapping
			WeaponSprees.Swap(0, SpreeIndex);
		}
	}
}

bool AUTPlayerState::CheckForMultiKill()
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL && GetWorld()->TimeSeconds - LastKillTime < GS->MultiKillDelay)
	{
		FName MKStat[4] = { NAME_MultiKillLevel0, NAME_MultiKillLevel1, NAME_MultiKillLevel2, NAME_MultiKillLevel3 };
		ModifyStatsValue(MKStat[FMath::Min(MultiKillLevel, 3)], 1);
		MultiKillLevel++;
		AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetOwner());
		if (MyPC != NULL)
		{
			MyPC->SendPersonalMessage(GS->MultiKillMessageClass, MultiKillLevel - 1, this, NULL);
		}

		AddCoolFactorEvent(MultiKillLevel * 100.0f);
		return true;
	}
	MultiKillLevel = 0;
	return false;
}

int32 AUTPlayerState::IncrementWeaponSpree(TSubclassOf<UUTDamageType> UTDamage)
{
	if (UTDamage)
	{
		if (UTDamage.GetDefaultObject()->SpreeSoundName != NAME_None)
		{
			int32 SpreeIndex = -1;
			for (int32 i = 0; i < WeaponSprees.Num(); i++)
			{
				if (WeaponSprees[i].SpreeSoundName == UTDamage.GetDefaultObject()->SpreeSoundName)
				{
					SpreeIndex = i;
					break;
				}
			}
			if (SpreeIndex == -1)
			{
				new(WeaponSprees)FWeaponSpree(UTDamage.GetDefaultObject()->SpreeSoundName);
				SpreeIndex = WeaponSprees.Num() - 1;
			}

			WeaponSprees[SpreeIndex].Kills++;
			return SpreeIndex;
		}
	}
	return -1;
}

void AUTPlayerState::IncrementKills(TSubclassOf<UDamageType> DamageType, bool bEnemyKill, AUTPlayerState* VictimPS)
{
	if (bEnemyKill)
	{
		AddCoolFactorEvent(100.0f);

		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
		AController* Controller = Cast<AController>(GetOwner());
		AUTCharacter* UTChar = GetUTCharacter();
		APawn* Pawn = UTChar ? UTChar : (Controller ? Controller->GetPawn() : nullptr);
		bool bShouldTauntKill = false;
		AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetOwner());
		TSubclassOf<UUTDamageType> UTDamage(*DamageType);

		if (CheckForMultiKill())
		{
			bShouldTauntKill = true;
			if (GM)
			{
				GM->AddMultiKillEventToReplay(Controller, FMath::Min(MultiKillLevel - 1, 3));
			}
		}

		bAnnounceWeaponSpree = false;
		if (UTDamage)
		{
			if (!UTDamage.GetDefaultObject()->StatsName.IsEmpty())
			{
				// FIXMESTEVE - preset, not constructed FName
				ModifyStatsValue(FName(*(UTDamage.GetDefaultObject()->StatsName + TEXT("Kills"))), 1);
			}
			
			int32 SpreeIndex = IncrementWeaponSpree(UTDamage);
			if (SpreeIndex >= 0)
			{
				// delay actual announcement to keep multiple announcements in preferred order
				bAnnounceWeaponSpree = (WeaponSprees[SpreeIndex].Kills == UTDamage.GetDefaultObject()->WeaponSpreeCount);
				bShouldTauntKill = bShouldTauntKill || bAnnounceWeaponSpree;
				// more likely to kill again with same weapon, so shorten search through array by swapping
				WeaponSprees.Swap(0, SpreeIndex);
			}
			if (UTDamage.GetDefaultObject()->RewardAnnouncementClass)
			{
				bShouldTauntKill = true;
				if (MyPC != nullptr)
				{
					MyPC->UTPlayerState->AddCoolFactorMinorEvent();
					MyPC->SendPersonalMessage(UTDamage.GetDefaultObject()->RewardAnnouncementClass, 0, this, VictimPS);
				}
			}
		}
		if (Pawn != NULL)
		{
			Spree++;
			if (Spree % 5 == 0)
			{
				FName SKStat[5] = { NAME_SpreeKillLevel0, NAME_SpreeKillLevel1, NAME_SpreeKillLevel2, NAME_SpreeKillLevel3, NAME_SpreeKillLevel4 };
				ModifyStatsValue(SKStat[FMath::Min(Spree / 5 - 1, 4)], 1);
				bShouldTauntKill = true;

				AUTBaseGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();

				if (GameMode != NULL)
				{
					GameMode->BroadcastLocalized(GetOwner(), GS->SpreeMessageClass, FMath::Min(Spree / 5, 5), this, VictimPS);
				}

				if (UTChar != NULL && UTChar->IsWearingAnyCosmetic())
				{
					UTChar->CosmeticSpreeCount = FMath::Min(Spree / 5, 5);
					UTChar->OnRepCosmeticSpreeCount();
				}

				AddCoolFactorEvent(Spree * 20.0f);

				if (GM)
				{
					GM->AddSpreeKillEventToReplay(Controller, FMath::Min(Spree / 5, 5));
				}
			}
		}

		if (UTChar != NULL && UTChar->IsWearingAnyCosmetic())
		{
			UTChar->LastCosmeticFlashTime = GetWorld()->TimeSeconds;
			UTChar->CosmeticFlashCount++;
			UTChar->OnRepCosmeticFlashCount();
		}

		LastKillTime = GetWorld()->TimeSeconds;
		Kills++;
		RoundKills++;
		ThisLifeKills++;

		if (bAnnounceWeaponSpree)
		{
			AnnounceWeaponSpree(UTDamage);
		}
		bool bVictimIsLocalPlayer = (Cast<AUTPlayerController>(VictimPS->GetOwner()) != NULL);
		bShouldTauntKill = bShouldTauntKill || (GM && (GetWorld()->GetTimeSeconds() - GM->LastGlobalTauntTime > 30.f));
		bVictimIsLocalPlayer = bVictimIsLocalPlayer && (GetWorld()->GetNetMode() == NM_Standalone);
		if (bShouldTauntKill && Controller)
		{
			if (bVictimIsLocalPlayer || ((GetWorld()->GetTimeSeconds() - LastTauntTime > 6.f) && GM && (GetWorld()->GetTimeSeconds() - GM->LastGlobalTauntTime > 3.f)))
			{
				LastTauntTime = GetWorld()->GetTimeSeconds();
				GM->LastGlobalTauntTime = LastTauntTime;
				if (!GetWorldTimerManager().IsTimerActive(PlayKillAnnouncement))
				{
					GetWorldTimerManager().SetTimer(PlayKillAnnouncement, this, &AUTPlayerState::AnnounceKill, 0.7f, false);
				}
			}
		}
		ModifyStatsValue(NAME_Kills, 1);
	}
	else
	{
		ModifyStatsValue(NAME_Suicides, 1);
	}
}

void AUTPlayerState::AnnounceWeaponSpree(TSubclassOf<UUTDamageType> UTDamage)
{
	// will be replicated to owning player, causing OnWeaponSpreeDamage()
	WeaponSpreeDamage = UTDamage;
	
	// for standalone
	AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetOwner());
	if (MyPC && MyPC->IsLocalPlayerController())
	{
		OnWeaponSpreeDamage();
	}
}

void AUTPlayerState::OnWeaponSpreeDamage()
{
	// received replicated 
	AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetOwner());
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (MyPC && GS && WeaponSpreeDamage)
	{
		MyPC->SendPersonalMessage(GS->SpreeMessageClass, 99, this, NULL, this);
	}
}

void AUTPlayerState::IncrementDeaths(TSubclassOf<UDamageType> DamageType, AUTPlayerState* KillerPlayerState)
{
	Deaths += 1;

	ModifyStatsValue(NAME_Deaths, 1);
	TSubclassOf<UUTDamageType> UTDamage(*DamageType);
	if (UTDamage && !UTDamage.GetDefaultObject()->StatsName.IsEmpty())
	{
		// FIXMESTEVE - preset, not constructed FName
		ModifyStatsValue(FName(*(UTDamage.GetDefaultObject()->StatsName + TEXT("Deaths"))), 1);
	}

	// spree has ended
	AUTBaseGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (Spree >= 5 && GameMode && GS)
	{
		GameMode->BroadcastLocalized(GetOwner(), GS->SpreeMessageClass, Spree / -5, this, KillerPlayerState);
	}
	Spree = 0;

	SetNetUpdateTime(FMath::Min(NetUpdateTime, GetWorld()->TimeSeconds + 0.3f * FMath::FRand()));

	if (Role == ROLE_Authority)
	{
		// Trigger it locally
		OnDeathsReceived();
	}
}

void AUTPlayerState::AdjustScore(int32 ScoreAdjustment)
{
	Score += ScoreAdjustment;
	ForceNetUpdate();
}

void AUTPlayerState::OnRespawnWaitReceived()
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState != NULL)
	{
		RespawnTime = UTGameState->GetRespawnWaitTimeFor(this);
		ForceRespawnTime = RespawnTime + UTGameState->ForceRespawnTime;
	}
}

void AUTPlayerState::OnDeathsReceived()
{
	OnRespawnWaitReceived();
}

void AUTPlayerState::SetOutOfLives(bool bNewValue)
{
	bOutOfLives = bNewValue;
	ForceNetUpdate();
	OnOutOfLives();
}

void AUTPlayerState::OnOutOfLives()
{
	AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetOwner());
	UUTLocalPlayer* UTLP = MyPC ? Cast<UUTLocalPlayer>(MyPC->Player) : NULL;
	if (UTLP)
	{
		if (bOutOfLives)
		{
			UTLP->OpenSpectatorWindow();
			if (MyPC && MyPC->MyUTHUD && MyPC->MyUTHUD->GetSpectatorSlideOut())
			{
				MyPC->MyUTHUD->GetSpectatorSlideOut()->SetMouseInteractive(true);
			}
		}
		else
		{
			UTLP->CloseSpectatorWindow();
		}
	}
}

void AUTPlayerState::BeginRallyTo(AUTRallyPoint* RallyTarget, const FVector& NewRallyLocation, float Delay)
{
	if (!GetWorldTimerManager().IsTimerActive(RallyTimerHandle))
	{
		GetWorldTimerManager().SetTimer(RallyTimerHandle, this, &AUTPlayerState::CompleteRally, Delay, false);
	}
	AUTPlayerController* PC = Cast<AUTPlayerController>(GetOwner());
	if (PC != nullptr)
	{
		PC->ClientStartRally(RallyTarget, NewRallyLocation, Delay);
	}
}

void AUTPlayerState::CompleteRally()
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode && GameMode->IsMatchInProgress() && GS && !GS->IsMatchIntermission())
	{
		GameMode->CompleteRallyRequest(Cast<AController>(GetOwner()));
		AUTPlayerController* PC = Cast<AUTPlayerController>(GetOwner());
		if (PC != nullptr)
		{
			PC->ClientCompleteRally();
		}
		else
		{
			AUTBot* B = Cast<AUTBot>(GetOwner());
			if (B != nullptr)
			{
				B->WhatToDoNext();
			}
		}
	}
}

void AUTPlayerState::SetRemainingBoosts(uint8 NewRemainingBoosts)
{
	if (Role == ROLE_Authority)
	{
		RemainingBoosts = NewRemainingBoosts;
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == nullptr || RemainingBoosts >= GS->BoostRechargeMaxCharges)
		{
			BoostRechargePct = 0.0f;
		}
	}
}

void AUTPlayerState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Role == ROLE_Authority)
	{
		AUTCharacter* UTChar = GetUTCharacter();
		bool bOldCanRally = bCanRally;
		bCanRally = (GetWorld()->GetTimeSeconds() > NextRallyTime) && UTChar && (UTChar->bCanRally || UTChar->GetCarriedObject());
		RemainingRallyDelay = (NextRallyTime > GetWorld()->GetTimeSeconds()) ? FMath::Clamp(int32(NextRallyTime - GetWorld()->GetTimeSeconds()), 0, 255) : 0;
		if (!bOldCanRally && bCanRally && !UTChar->GetCarriedObject())
		{
			// notify of transition if rally is available
			AUTFlagRunGameState* GS = GetWorld()->GetGameState<AUTFlagRunGameState>();
			AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetOwner());
			if (GS && GS->bAttackersCanRally && MyPC && Team && (GS->bRedToCap == (Team->TeamIndex == 0)))
			{
				MyPC->ClientReceiveLocalizedMessage(UUTCTFMajorMessage::StaticClass(), 30, this);
				AUTPlayerState* FC = GS->GetFlagHolder(Team->TeamIndex);
				if (FC)
				{
					FC->AnnounceStatus(StatusMessage::RallyNow);
				}
			}
		}
		if (!StatsID.IsEmpty() && !RequestedName.IsEmpty() && Cast<AController>(GetOwner()))
		{
			AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
			if (UTGameState)
			{
				TSharedRef<const FUniqueNetId> UserId = MakeShareable(new FUniqueNetIdString(*StatsID));
				FText EpicAccountName = UTGameState->GetEpicAccountNameForAccount(UserId);
				if (!EpicAccountName.IsEmpty())
				{
					GetWorld()->GetAuthGameMode()->ChangeName(Cast<AController>(GetOwner()), RequestedName, false);
					RequestedName = TEXT("");
				}
			}
		}
	}
	// If we are waiting to respawn then count down
	RespawnTime -= DeltaTime;
	ForceRespawnTime -= DeltaTime;

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL && GS->IsMatchInProgress() && !GS->IsMatchIntermission() && !GS->IsMatchInCountdown() && GS->BoostRechargeTime > 0.0f && RemainingBoosts < GS->BoostRechargeMaxCharges)
	{
		BoostRechargePct += DeltaTime / GS->BoostRechargeTime;
		if (BoostRechargePct >= 1.0f)
		{
			BoostRechargePct -= 1.0f;
			SetRemainingBoosts(RemainingBoosts + 1); // note: will set BoostRechargeTimeRemaining to zero if we're no longer allowed to recharge
		}
	}

	if (CurrentCoolFactor > 0.0f)
	{
		CurrentCoolFactor = FMath::Max(0.f, CurrentCoolFactor - CoolFactorBleedSpeed * DeltaTime);
	}
}

AUTCharacter* AUTPlayerState::GetUTCharacter()
{
	AController* Controller = Cast<AController>(GetOwner());
	if (Controller != NULL)
	{
		AUTCharacter* UTChar = Cast<AUTCharacter>(Controller->GetPawn());
		if (UTChar != NULL)
		{
			return UTChar;
		}
	}
	
	// iterate through all pawns and find matching playerstate ref
	// note: this is set up to use the old character as long as possible after death, until the player respawns
	if (CachedCharacter == NULL || CachedCharacter->IsDead() || CachedCharacter->PlayerState != this)
	{
		for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
		{
			AUTCharacter* UTChar = Cast<AUTCharacter>(*Iterator);
			if (UTChar != NULL && UTChar->PlayerState == this && !UTChar->IsDead())
			{
				CachedCharacter = UTChar;
				return UTChar;
			}
		}
	}

	return CachedCharacter;
}

void AUTPlayerState::UpdateWeaponSkinPrefFromProfile(AUTWeapon* Weapon)
{
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(GetOwner());
	if (PC)
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
		if (LP)
		{
			UUTProfileSettings* ProfileSettings = LP->GetProfileSettings();
			if (ProfileSettings)
			{
				FString WeaponSkinClassname = ProfileSettings->GetWeaponSkinClassname(Weapon);
				if (!WeaponSkinClassname.IsEmpty())
				{
					ServerReceiveWeaponSkin(WeaponSkinClassname);
				}
			}
		}
	}
}

void AUTPlayerState::ServerReceiveWeaponSkin_Implementation(const FString& NewWeaponSkin)
{
#if WITH_EDITOR
	// Intentionally break this in network games until hitches can get fixed
	if (NM_Standalone != GetNetMode())
	{
		return;
	}
#endif

	if (!bOnlySpectator)
	{
		UUTWeaponSkin* WeaponSkin = LoadObject<UUTWeaponSkin>(NULL, *NewWeaponSkin, NULL, GetCosmeticLoadFlags(), NULL);
		
		if (WeaponSkin != nullptr && ValidateEntitlementSingleObject(WeaponSkin))
		{
			bool bAlreadyAssigned = false;
			// Verify we don't already have a skin for this weapon
			for (int32 i = 0; i < WeaponSkins.Num(); i++)
			{
				if (WeaponSkins[i]->WeaponType == WeaponSkin->WeaponType)
				{
					WeaponSkins[i] = WeaponSkin;
					bAlreadyAssigned = true;
				}
			}

			if (!bAlreadyAssigned)
			{
				WeaponSkins.Add(WeaponSkin);
			}

			AUTCharacter* UTChar = GetUTCharacter();
			if (UTChar != nullptr)
			{
				UTChar->SetSkinForWeapon(WeaponSkin);
			}
		}
	}
}

bool AUTPlayerState::ServerReceiveWeaponSkin_Validate(const FString& NewWeaponSkin)
{
	return true;
}

void AUTPlayerState::OnRepHat()
{
	AUTCharacter* UTChar = GetUTCharacter();
	if (UTChar != nullptr)
	{
		UTChar->SetHatClass(HatClass);
	}
}

void AUTPlayerState::OnRepHatLeader()
{
	AUTCharacter* UTChar = GetUTCharacter();
	if (UTChar != nullptr)
	{
		if (UTChar->LeaderHat && LeaderHatClass != UTChar->LeaderHat->GetClass())
		{
			UTChar->LeaderHat->Destroy();
			UTChar->LeaderHat = nullptr;
			UTChar->HasHighScoreChanged();
		}
	}
}

void AUTPlayerState::OnRepHatVariant()
{
	AUTCharacter* UTChar = GetUTCharacter();
	if (UTChar != nullptr)
	{
		UTChar->SetHatVariant(HatVariant);
	}
}

void AUTPlayerState::OnRepEyewear()
{
	AUTCharacter* UTChar = GetUTCharacter();
	if (UTChar != nullptr)
	{
		UTChar->SetEyewearClass(EyewearClass);
	}
}

void AUTPlayerState::OnRepEyewearVariant()
{
	AUTCharacter* UTChar = GetUTCharacter();
	if (UTChar != nullptr)
	{
		UTChar->SetEyewearVariant(EyewearVariant);
	}
}

void AUTPlayerState::ServerReceiveHatVariant_Implementation(int32 NewVariant)
{
	HatVariant = NewVariant;
	OnRepHatVariant();
}

bool AUTPlayerState::ServerReceiveHatVariant_Validate(int32 NewVariant)
{
	return true;
}

void AUTPlayerState::ServerReceiveEyewearVariant_Implementation(int32 NewVariant)
{
	EyewearVariant = NewVariant;
	OnRepEyewearVariant();
}

bool AUTPlayerState::ServerReceiveEyewearVariant_Validate(int32 NewVariant)
{
	return true;
}

void AUTPlayerState::ServerReceiveHatClass_Implementation(const FString& NewHatClass)
{
	if (!bOnlySpectator && (GetWorld()->IsPlayInEditor() || GetNetMode() == NM_Standalone || HatClass == NULL || !GetWorld()->GetGameState()->HasMatchStarted()))
	{
		if (!NewHatClass.IsEmpty())
		{
			HatClass = LoadClass<AUTHat>(NULL, *NewHatClass, NULL, GetCosmeticLoadFlags(), NULL);
		}

		// Allow the game mode to validate the hat.
		AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (GameMode && !GameMode->ValidateHat(this, NewHatClass))
		{
			return;
		}

		if (!GetWorld()->IsPlayInEditor() && HatClass->IsChildOf(AUTHatLeader::StaticClass()))
		{
			HatClass = nullptr;
		}

		if (NewHatClass.IsEmpty())
		{
			HatClass = nullptr;
		}

		if (HatClass != nullptr)
		{
			ValidateEntitlements();

			OnRepHat();
		}
	}
}

bool AUTPlayerState::ServerReceiveHatClass_Validate(const FString& NewHatClass)
{
	return true;
}

void AUTPlayerState::ServerReceiveLeaderHatClass_Implementation(const FString& NewLeaderHatClass)
{
	if (!bOnlySpectator && (GetNetMode() == NM_Standalone || LeaderHatClass == NULL || !GetWorld()->GetGameState()->HasMatchStarted()))
	{
		if (!NewLeaderHatClass.IsEmpty())
		{
			LeaderHatClass = LoadClass<AUTHatLeader>(NULL, *NewLeaderHatClass, NULL, GetCosmeticLoadFlags(), NULL);
		}

		if (NewLeaderHatClass.IsEmpty())
		{
			LeaderHatClass = nullptr;
		}

		if (LeaderHatClass != nullptr)
		{
			ValidateEntitlements();
		}
	}
}

bool AUTPlayerState::ServerReceiveLeaderHatClass_Validate(const FString& NewHatClass)
{
	return true;
}

void AUTPlayerState::ServerReceiveEyewearClass_Implementation(const FString& NewEyewearClass)
{
	if (!bOnlySpectator && (GetNetMode() == NM_Standalone || EyewearClass == NULL || !GetWorld()->GetGameState()->HasMatchStarted()))
	{
		if (!NewEyewearClass.IsEmpty())
		{
			EyewearClass = LoadClass<AUTEyewear>(NULL, *NewEyewearClass, NULL, GetCosmeticLoadFlags(), NULL);
		}

		if (NewEyewearClass.IsEmpty())
		{
			EyewearClass = nullptr;
		}

		OnRepEyewear();
		if (EyewearClass != NULL)
		{
			ValidateEntitlements();
		}
	}
}

bool AUTPlayerState::ServerReceiveEyewearClass_Validate(const FString& NewEyewearClass)
{
	return true;
}

void AUTPlayerState::ServerReceiveGroupTauntClass_Implementation(const FString& NewGroupTauntClass)
{
	if (!bOnlySpectator)
	{
		GroupTauntClass = LoadClass<AUTGroupTaunt>(NULL, *NewGroupTauntClass, NULL, GetCosmeticLoadFlags(), NULL);
		if (GroupTauntClass != NULL)
		{
			ValidateEntitlements();
		}
	}
}

bool AUTPlayerState::ServerReceiveGroupTauntClass_Validate(const FString& NewEyewearClass)
{
	return true;
}

void AUTPlayerState::ServerReceiveTauntClass_Implementation(const FString& NewTauntClass)
{
	if (!bOnlySpectator)
	{
		TauntClass = LoadClass<AUTTaunt>(NULL, *NewTauntClass, NULL, GetCosmeticLoadFlags(), NULL);
		if (TauntClass != NULL)
		{
			ValidateEntitlements();
		}
	}
}

bool AUTPlayerState::ServerReceiveTauntClass_Validate(const FString& NewTauntClass)
{
	return true;
}

void AUTPlayerState::ServerReceiveTaunt2Class_Implementation(const FString& NewTauntClass)
{
	if (!bOnlySpectator)
	{
		Taunt2Class = LoadClass<AUTTaunt>(NULL, *NewTauntClass, NULL, GetCosmeticLoadFlags(), NULL);
		if (Taunt2Class != NULL)
		{
			ValidateEntitlements();
		}
	}
}

bool AUTPlayerState::ServerReceiveTaunt2Class_Validate(const FString& NewTauntClass)
{
	return true;
}

void AUTPlayerState::HandleTeamChanged(AController* Controller)
{
	AUTCharacter* Pawn = GetUTCharacter();
	if (Pawn != NULL)
	{
		Pawn->PlayerChangedTeam();
	}
	if (Team)
	{
		int32 Switch = (Team->TeamIndex == 0) ? 9 : 10;
		AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
		if (PC)
		{
			PC->ClientReceiveLocalizedMessage(UUTGameMessage::StaticClass(), Switch, this, NULL, NULL);
		}
	}
}

void AUTPlayerState::ServerRequestChangeTeam_Implementation(uint8 NewTeamIndex)
{
	AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (Game != NULL && Game->bTeamGame)
	{
		AController* Controller =  Cast<AController>( GetOwner() );
		if (Controller != NULL)
		{
			if (NewTeamIndex == 255 && Team != NULL)
			{
				NewTeamIndex = (Team->TeamIndex + 1) % FMath::Max<uint8>(1, GetWorld()->GetGameState<AUTGameState>()->Teams.Num());
			}
			if (Game->ChangeTeam(Controller, NewTeamIndex, true))
			{
				HandleTeamChanged(Controller);
			}
		}
	}
}

bool AUTPlayerState::ServerRequestChangeTeam_Validate(uint8 FireModeNum)
{
	return true;
}

void AUTPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL)
	{
		if (PS->Team == nullptr)
		{
			// MovePlayerToTeam(AController* Player, AUTPlayerState* PS, uint8 NewTeam
		}
		PS->Team = Team;
		PS->bReadStatsFromCloud = bReadStatsFromCloud;
		PS->bSuccessfullyReadStatsFromCloud = bSuccessfullyReadStatsFromCloud;
		PS->bWroteStatsToCloud = bWroteStatsToCloud;
		PS->PartySize = PartySize;
		PS->StatsID = StatsID;
		PS->Kills = Kills;
		PS->RoundKills = RoundKills;
		PS->DamageDone = DamageDone;
		PS->Deaths = Deaths;
		PS->Assists = Assists;
		PS->HatClass = HatClass;
		PS->LeaderHatClass = LeaderHatClass;
		PS->EyewearClass = EyewearClass;
		PS->HatVariant = HatVariant;
		PS->EyewearVariant = EyewearVariant;
		PS->SelectedCharacter = SelectedCharacter;
		PS->StatManager = StatManager;
		PS->StatsData = StatsData;
		PS->TauntClass = TauntClass;
		PS->Taunt2Class = Taunt2Class;
		PS->bSkipELO = bSkipELO;
		PS->BoostClass = BoostClass;
		PS->RemainingBoosts = RemainingBoosts;
		PS->RemainingLives = RemainingLives;
		PS->bOutOfLives = bOutOfLives;
		PS->BoostRechargePct = BoostRechargePct;
		if (PS->StatManager)
		{
			PS->StatManager->InitializeManager(PS);
		}
	}
}
void AUTPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL)
	{
		// note that we don't need to call Add/RemoveFromTeam() here as that happened when Team was assigned to the passed in PlayerState
		Team = PS->Team;

		SpectatingID = PS->SpectatingID;
		SpectatingIDTeam = PS->SpectatingIDTeam;

#if WITH_PROFILE
		if (PS->McpProfile != NULL)
		{
			FMcpProfileSetter::Set(this, PS->GetMcpProfile());
		}
#endif
	}
}

void AUTPlayerState::OnCarriedObjectChanged()
{
	SetCarriedObject(CarriedObject);
}

void AUTPlayerState::SetCarriedObject(AUTCarriedObject* NewCarriedObject)
{
	if (Role == ROLE_Authority)
	{
		CarriedObject = NewCarriedObject;
		ForceNetUpdate();
	}
	AUTPlayerController *PlayerOwner = Cast<AUTPlayerController>(GetOwner());
	if (PlayerOwner && PlayerOwner->MyUTHUD && CarriedObject)
	{
		PlayerOwner->MyUTHUD->LastFlagGrabTime = GetWorld()->GetTimeSeconds();
	}
}

void AUTPlayerState::ClearCarriedObject(AUTCarriedObject* OldCarriedObject)
{
	if (Role == ROLE_Authority)
	{
		if (CarriedObject == OldCarriedObject)
		{
			CarriedObject = NULL;
			ForceNetUpdate();
		}
	}
}

uint8 AUTPlayerState::GetTeamNum() const
{
	return (Team != NULL) ? Team->GetTeamNum() : 255;
}

void AUTPlayerState::EndPlay(const EEndPlayReason::Type Reason)
{
	if (!bIsInactive && Team != NULL && GetOwner() != NULL)
	{
		Team->RemoveFromTeam(Cast<AController>(GetOwner()));
	}
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(Reason);
}

void AUTPlayerState::BeginPlay()
{
	// default so value is never NULL
	if (SelectedCharacter == NULL)
	{
		SelectedCharacter = GetDefault<AUTCharacter>()->CharacterData;
	}

	// test code for editor/PIE where there's no profile
#if WITH_EDITOR
	if (GIsEditor)
	{
		TauntClass = LoadClass<AUTTaunt>(NULL, TEXT("/Game/RestrictedAssets/Blueprints/Taunts/Taunt_PelvicThrust.Taunt_PelvicThrust_C"), NULL, GetCosmeticLoadFlags(), NULL);
	}
#endif

	Super::BeginPlay();

	if (Role == ROLE_Authority && StatManager == nullptr)
	{
		//Make me a statmanager
		StatManager = NewObject<UStatManager>(this, UStatManager::StaticClass());
		StatManager->InitializeManager(this);
	}

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
		OnlineUserCloudInterface = OnlineSubsystem->GetUserCloudInterface();

		if (Role == ROLE_Authority && OnlineUserCloudInterface.IsValid())
		{
			OnReadUserFileCompleteDelegate = FOnReadUserFileCompleteDelegate::CreateUObject(this, &AUTPlayerState::OnReadUserFileComplete);
			OnlineUserCloudInterface->AddOnReadUserFileCompleteDelegate_Handle(OnReadUserFileCompleteDelegate);

			OnWriteUserFileCompleteDelegate = FOnWriteUserFileCompleteDelegate::CreateUObject(this, &AUTPlayerState::OnWriteUserFileComplete);
			OnlineUserCloudInterface->AddOnWriteUserFileCompleteDelegate_Handle(OnWriteUserFileCompleteDelegate);
		}
	}
}

void AUTPlayerState::SetCharacterVoice(const FString& CharacterVoicePath)
{
	if (Role == ROLE_Authority)
	{
		CharacterVoice = (CharacterVoicePath.Len() > 0) ? LoadClass<UUTCharacterVoice>(NULL, *CharacterVoicePath, NULL, GetCosmeticLoadFlags(), NULL) : NULL;
		// redirect from blueprint, for easier testing in the editor via C/P
#if WITH_EDITORONLY_DATA
		if (CharacterVoice == NULL)
		{
			UBlueprint* BP = LoadObject<UBlueprint>(NULL, *CharacterVoicePath, NULL, GetCosmeticLoadFlags(), NULL);
			if (BP != NULL)
			{
				CharacterVoice = *BP->GeneratedClass;
			}
		}
#endif
	}
}

bool AUTPlayerState::IsOwnedByReplayController() const
{
	return Cast<AUTDemoRecSpectator>(GetOwner()) != nullptr;
}

void AUTPlayerState::SetCharacter(const FString& CharacterPath)
{
	if (Role == ROLE_Authority || IsOwnedByReplayController())
	{
		TSubclassOf<AUTCharacterContent> NewCharacter = (CharacterPath.Len() > 0) ? TSubclassOf<AUTCharacterContent>(FindObject<UClass>(NULL, *CharacterPath, false)) : GetDefault<AUTCharacter>()->CharacterData;
// redirect from blueprint, for easier testing in the editor via C/P
#if WITH_EDITORONLY_DATA
		if (NewCharacter == NULL && CharacterPath.Len() > 0 && GetNetMode() == NM_Standalone)
		{
			UBlueprint* BP = LoadObject<UBlueprint>(NULL, *CharacterPath, NULL, LOAD_NoWarn, NULL);
			if (BP != NULL)
			{
				NewCharacter = *BP->GeneratedClass;
			}
		}
#endif
		if (NewCharacter == NULL)
		{
			// async load the character class
			UObject* CharPkg = NULL;
			FString Path = CharacterPath;
			if (ResolveName(CharPkg, Path, true, false) && Cast<UPackage>(CharPkg) != NULL)
			{
				TWeakObjectPtr<AUTPlayerState> PS(this);
				if (FPackageName::DoesPackageExist(CharPkg->GetName()))
				{
					LoadPackageAsync(CharPkg->GetName(), FLoadPackageAsyncDelegate::CreateLambda([PS, CharacterPath](const FName& PackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
					{
						if (Result == EAsyncLoadingResult::Succeeded && PS.IsValid() && TSubclassOf<AUTCharacterContent>(FindObject<UClass>(NULL, *CharacterPath, false)) != NULL)
						{
							PS->SetCharacter(CharacterPath);
						}
						else
						{
							// redirectors don't work when using FindObject, handle that case
							UObjectRedirector* Redirector = FindObject<UObjectRedirector>(NULL, *CharacterPath, true);
							if (Redirector != NULL && Redirector->DestinationObject != NULL)
							{
								PS->SetCharacter(Redirector->DestinationObject->GetPathName());
							}
						}
					}));
				}
			}
		}
		// make sure it's not an invalid base class
		else if (NewCharacter != NULL && !(NewCharacter->ClassFlags & CLASS_Abstract) && NewCharacter.GetDefaultObject()->GetMesh()->SkeletalMesh != NULL)
		{
			SelectedCharacter = NewCharacter;
			NotifyTeamChanged();
		}
	}
}

bool AUTPlayerState::ServerSetCharacter_Validate(const FString& CharacterPath)
{
	return true;
}
void AUTPlayerState::ServerSetCharacter_Implementation(const FString& CharacterPath)
{
#if WITH_EDITOR
	// Don't allow character loading during join in progress right now, it causes poor performance
	if (!bOnlySpectator && (GetNetMode() == NM_Standalone || !GetWorld()->GetGameState()->HasMatchStarted() || (nullptr != GetWorld()->GetAuthGameMode<AUTLobbyGameMode>())))
#else
	if (!bOnlySpectator && (GetNetMode() == NM_Standalone || SelectedCharacter == NULL || !GetWorld()->GetGameState()->HasMatchStarted()))
#endif
	{
		AUTCharacter* MyPawn = GetUTCharacter();
		// suicide if feign death because the mesh reset causes physics issues
		if (MyPawn != NULL && MyPawn->IsFeigningDeath())
		{
			MyPawn->PlayerSuicide();
		}
		SetCharacter(CharacterPath);
		if (CharacterPath.Len() > 0)
		{
			ValidateEntitlements();
		}
	}
}

bool AUTPlayerState::ModifyStat(FName StatName, int32 Amount, EStatMod::Type ModType)
{
	if (StatManager != nullptr)
	{
		FStat* Stat = StatManager->GetStatByName(StatName);
		if (Stat != NULL)
		{
			Stat->ModifyStat(Amount, ModType);
			if (ModType == EStatMod::Delta && Amount > 0)
			{
				// award XP
				GiveXP(Stat->XPPerPoint * Amount);
			}
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

bool AUTPlayerState::ServerNextChatDestination_Validate() { return true; }
void AUTPlayerState::ServerNextChatDestination_Implementation()
{
	AUTBaseGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
	if (GameMode)
	{
		ChatDestination = GameMode->GetNextChatDestination(this, ChatDestination);
	}
}

void AUTPlayerState::ProfileNotification(const FOnlineNotification& Notification)
{
	if (Role == ROLE_Authority)
	{
		// route to client
		AUTPlayerController* PC = Cast<AUTPlayerController>(GetOwner());
		if (PC != NULL && Notification.Payload.IsValid())
		{
			FString OutputJsonString;
			TSharedRef< TJsonWriter< TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > Writer = TJsonWriterFactory< TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&OutputJsonString);
			FJsonSerializer::Serialize(Notification.Payload->AsObject().ToSharedRef(), Writer);
			PC->ClientBackendNotify(Notification.TypeStr, OutputJsonString);
		}
	}
}

void AUTPlayerState::ProfileItemsChanged(const TSet<FString>& ChangedTypes, int64 ProfileRevision)
{
	ValidateEntitlements();
}

void AUTPlayerState::GiveXP(const FXPBreakdown& AddXP)
{
	AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (Role == ROLE_Authority && Game != NULL)
	{
		XP += (AddXP * Game->XPMultiplier);
	}
}

void AUTPlayerState::ClampXP(int32 MaxValue)
{
	float Percent = float(XP.Total()) / float(MaxValue);
	if (Percent > 1.0f)
	{
		XP /= Percent;
	}
}

void AUTPlayerState::ReadStatsFromCloud()
{
	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode == nullptr || GameMode->bDisableCloudStats)
	{
		return;
	}

	// Don't read stats from cloud if we've already written them, consider memory to be a valid representation of the stats
	if (!StatsID.IsEmpty() && OnlineUserCloudInterface.IsValid() && !bWroteStatsToCloud && !bOnlySpectator && StatManager && !GetWorld()->IsPlayInEditor() && !bSuccessfullyReadStatsFromCloud)
	{
		OnlineUserCloudInterface->ReadUserFile(FUniqueNetIdString(*StatsID), GetStatsFilename());
	}
}

void AUTPlayerState::ReadMMRFromBackend()
{
	// get MCP Utils
	TSharedPtr<const FUniqueNetId> UserId = MakeShareable(new FUniqueNetIdString(*StatsID));
	UUTMcpUtils* McpUtils = UUTMcpUtils::Get(GetWorld(), UserId);
	if (McpUtils == nullptr)
	{
		UE_LOG(UT, Warning, TEXT("Unable to load McpUtils. Will not be able to read MMR from MCP"));
		return;
	}

	TArray<FString> MatchRatingTypes;
	MatchRatingTypes.Add(TEXT("SkillRating"));
	MatchRatingTypes.Add(TEXT("TDMSkillRating"));
	MatchRatingTypes.Add(TEXT("DMSkillRating"));
	MatchRatingTypes.Add(TEXT("CTFSkillRating"));
	MatchRatingTypes.Add(TEXT("ShowdownSkillRating"));
	MatchRatingTypes.Add(TEXT("FlagRunSkillRating"));
	MatchRatingTypes.Add(TEXT("RankedDuelSkillRating"));
	MatchRatingTypes.Add(TEXT("RankedCTFSkillRating"));
	MatchRatingTypes.Add(TEXT("RankedShowdownSkillRating"));
	// This should be a weak ptr here, but UTLocalPlayer is unlikely to go away
	TWeakObjectPtr<AUTPlayerState> WeakPlayerState(this);
	McpUtils->GetBulkAccountMmr(MatchRatingTypes, [WeakPlayerState](const FOnlineError& Result, const FBulkAccountMmr& Response)
	{
		if (!Result.bSucceeded)
		{
			// best we can do is log an error
			UE_LOG(UT, Warning, TEXT("Failed to read MMR from the server. (%d) %s %s"), Result.HttpResult, *Result.ErrorCode, *Result.ErrorMessage.ToString());
		}
		else
		{
			if (!WeakPlayerState.IsValid())
			{
				return;
			}

			WeakPlayerState->DuelRank = Response.Ratings[0];
			WeakPlayerState->TDMRank = Response.Ratings[1];
			WeakPlayerState->DMRank = Response.Ratings[2];
			WeakPlayerState->CTFRank = Response.Ratings[3];
			WeakPlayerState->ShowdownRank = Response.Ratings[4];
			WeakPlayerState->FlagRunRank = Response.Ratings[5];
			WeakPlayerState->RankedDuelRank = Response.Ratings[6];
			WeakPlayerState->RankedCTFRank = Response.Ratings[7];
			WeakPlayerState->RankedShowdownRank = Response.Ratings[8];

			WeakPlayerState->DuelMatchesPlayed = FMath::Min(255, Response.NumGamesPlayed[0]);
			WeakPlayerState->TDMMatchesPlayed = FMath::Min(255, Response.NumGamesPlayed[1]);
			WeakPlayerState->DMMatchesPlayed = FMath::Min(255, Response.NumGamesPlayed[2]);
			WeakPlayerState->CTFMatchesPlayed = FMath::Min(255, Response.NumGamesPlayed[3]);
			WeakPlayerState->ShowdownMatchesPlayed = FMath::Min(255, Response.NumGamesPlayed[4]);
			WeakPlayerState->FlagRunMatchesPlayed = FMath::Min(255, Response.NumGamesPlayed[5]);
			WeakPlayerState->RankedDuelMatchesPlayed = FMath::Min(255, Response.NumGamesPlayed[6]);
			WeakPlayerState->RankedCTFMatchesPlayed = FMath::Min(255, Response.NumGamesPlayed[7]);
			WeakPlayerState->RankedShowdownMatchesPlayed = FMath::Min(255, Response.NumGamesPlayed[8]);
			
			AUTBaseGameMode* BaseGame = WeakPlayerState->GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
			if (BaseGame)
			{
				BaseGame->ReceivedRankForPlayer(WeakPlayerState.Get());
			}

			UE_LOG(UT, Log, TEXT("%s MMR fetched from the backend (Duel:%d) (TDM:%d) (FFA:%d)"), *WeakPlayerState->PlayerName, WeakPlayerState->DuelRank, WeakPlayerState->TDMRank, WeakPlayerState->DMRank);
			UE_LOG(UT, Log, TEXT("(CTF:%d) (Showdown:%d) (Ranked Showdown:%d)"), WeakPlayerState->CTFRank, WeakPlayerState->ShowdownRank, WeakPlayerState->RankedShowdownRank);
			UE_LOG(UT, Log, TEXT("(Ranked Duel:%d) (Ranked CTF:%d)"), WeakPlayerState->RankedDuelRank, WeakPlayerState->RankedCTFRank);
		}
	});
}

void AUTPlayerState::OnReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	// this notification is for the file that we requested
	if (InUserId.ToString() == StatsID && FileName == GetStatsFilename() && StatManager)
	{
		// need to record that we at least finished our attempt to get stats to avoid quickly overwriting a player's stats with nothing
		bReadStatsFromCloud = true;

		if (bWasSuccessful)
		{
			bSuccessfullyReadStatsFromCloud = true;

			UE_LOG(UT, Log, TEXT("OnReadUserFileComplete bWasSuccessful:%d %s %s"), int32(bWasSuccessful), *InUserId.ToString(), *FileName);

			TArray<uint8> FileContents;
			if (OnlineUserCloudInterface->GetFileContents(InUserId, FileName, FileContents))
			{
				if (FileContents.GetData()[FileContents.Num() - 1] != 0)
				{
					UE_LOG(LogGameStats, Warning, TEXT("Failed to get proper stats json"));
					return;
				}

				FString JsonString = ANSI_TO_TCHAR((char*)FileContents.GetData());

				TSharedPtr<FJsonObject> StatsJson;
				TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
				if (FJsonSerializer::Deserialize(JsonReader, StatsJson) && StatsJson.IsValid())
				{
					FString JsonStatsID;
					if (StatsJson->TryGetStringField(TEXT("StatsID"), JsonStatsID) && JsonStatsID == StatsID)
					{
						UE_LOG(LogGameStats, Log, TEXT("Stats ID matched, adding stats from the cloud to current stats"));
					}
					else
					{
						UE_LOG(LogGameStats, Warning, TEXT("Failed to find matching StatsID in valid stats read."));
					}

					StatManager->InsertDataFromNonBackendJsonObject(StatsJson);
				}
			}
		}
	}
}

void AUTPlayerState::AddMatchHighlight(FName NewHighlight, float HighlightData)
{
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	float NewPriority = GameState ? GameState->HighlightPriority.FindRef(NewHighlight) : 0.f;

	for (int32 i = 0; i < 5; i++)
	{
		if (MatchHighlights[i] == NAME_None)
		{
			MatchHighlights[i] = NewHighlight;
			MatchHighlightData[i] = HighlightData;
			return;
		}
		else if (GameState)
		{
			float TestPriority = GameState->HighlightPriority.FindRef(MatchHighlights[i]);
			if (NewPriority > TestPriority)
			{
				// insert the highlight, look for a spot for the displaced highlight
				FName MovedHighlight = MatchHighlights[i];
				float MovedData = MatchHighlightData[i];
				MatchHighlights[i] = NewHighlight;
				MatchHighlightData[i] = HighlightData;
				NewHighlight = MovedHighlight;
				HighlightData = MovedData;
			}
		}
	}

	// if no open slots, try to replace lowest priority highlight
	if (GameState)
	{
		NewPriority = GameState->HighlightPriority.FindRef(NewHighlight);
		float WorstPriority = 0.f;
		int32 WorstIndex = -1.f;
		for (int32 i = 0; i < 5; i++)
		{
			float TestPriority = GameState->HighlightPriority.FindRef(MatchHighlights[i]);
			if (WorstPriority < TestPriority)
			{
				WorstPriority = TestPriority;
				WorstIndex = i;
			}
		}
		if ((WorstIndex >= 0) && (NewPriority < WorstPriority))
		{
			MatchHighlights[WorstIndex] = NewHighlight;
			MatchHighlightData[WorstIndex] = HighlightData;
		}
	}
}

void AUTPlayerState::OnWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	// this notification is for us
	if (InUserId.ToString() == StatsID && FileName == GetStatsFilename())
	{
		UE_LOG(LogGameStats, Log, TEXT("OnWriteUserFileComplete bWasSuccessful:%d %s %s"), int32(bWasSuccessful), *InUserId.ToString(), *FileName);
	}
}

FString AUTPlayerState::GetStatsFilename()
{
	return UUTLocalPlayer::GetStatsFilename(); //TEXT("stats.json");
}

void AUTPlayerState::WriteStatsToCloud()
{
	if (bWroteStatsToCloud)
	{
		UE_LOG(LogGameStats, Warning, TEXT("Attempted to write stats twice for %s, skipping"), *PlayerName);
		return;
	}

	if (!StatsID.IsEmpty() && StatManager != nullptr)
	{
		// Write the stats stored in the cloud file
		if (bReadStatsFromCloud && OnlineUserCloudInterface.IsValid() && !bOnlySpectator)
		{
			// We ended with this player name, save it in the stats
			StatManager->PreviousPlayerNames.AddUnique(PlayerName);
			if (StatManager->PreviousPlayerNames.Num() > StatManager->NumPreviousPlayerNamesToKeep)
			{
				StatManager->PreviousPlayerNames.RemoveAt(0, StatManager->PreviousPlayerNames.Num() - StatManager->NumPreviousPlayerNamesToKeep);
			}

			TArray<uint8> FileContents;
			TSharedPtr<FJsonObject> StatsJson = MakeShareable(new FJsonObject);
			StatsJson->SetStringField(TEXT("StatsID"), StatsID);
			StatsJson->SetStringField(TEXT("PlayerName"), PlayerName);
			StatManager->PopulateJsonObjectForNonBackendStats(StatsJson);

			FString OutputJsonString;
			TSharedRef< TJsonWriter< TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > Writer = TJsonWriterFactory< TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&OutputJsonString);
			FJsonSerializer::Serialize(StatsJson.ToSharedRef(), Writer);
			{
				FMemoryWriter MemoryWriter(FileContents);
				MemoryWriter.Serialize(TCHAR_TO_ANSI(*OutputJsonString), OutputJsonString.Len() + 1);
			}

			UE_LOG(LogGameStats, Log, TEXT("Writing stats for %s, previously read stats: %d"), *PlayerName, bSuccessfullyReadStatsFromCloud ? 1 : 0);
			UE_LOG(LogGameStats, VeryVerbose, TEXT("JSON: %s"), *OutputJsonString);

			OnlineUserCloudInterface->WriteUserFile(FUniqueNetIdString(*StatsID), GetStatsFilename(), FileContents);
			bWroteStatsToCloud = true;
		}

		// Write the stats going to the backend
		{
			TSharedPtr<FJsonObject> StatsJson = MakeShareable(new FJsonObject);
			StatManager->PopulateJsonObjectForBackendStats(StatsJson, this);
			FString OutputJsonString;
			TArray<uint8> BackendStatsData;
			TSharedRef< TJsonWriter< TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > Writer = TJsonWriterFactory< TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&OutputJsonString);
			FJsonSerializer::Serialize(StatsJson.ToSharedRef(), Writer);
			{
				FMemoryWriter MemoryWriter(BackendStatsData);
				MemoryWriter.Serialize(TCHAR_TO_ANSI(*OutputJsonString), OutputJsonString.Len() + 1);
			}

			FString BaseURL = GetBackendBaseUrl();
			FString CommandURL = TEXT("/api/stats/accountId/");
			FString FinalStatsURL = BaseURL + CommandURL + StatsID + TEXT("/bulk?ownertype=1");

			FHttpRequestPtr StatsWriteRequest = FHttpModule::Get().CreateRequest();
			if (StatsWriteRequest.IsValid())
			{
				StatsWriteRequest->SetURL(FinalStatsURL);
				StatsWriteRequest->OnProcessRequestComplete().BindUObject(this, &AUTPlayerState::StatsWriteComplete);
				StatsWriteRequest->SetVerb(TEXT("POST"));
				StatsWriteRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
				
				if (OnlineIdentityInterface.IsValid())
				{
					FString AuthToken = OnlineIdentityInterface->GetAuthToken(0);
					StatsWriteRequest->SetHeader(TEXT("Authorization"), FString(TEXT("bearer ")) + AuthToken);
				}

				UE_LOG(LogGameStats, VeryVerbose, TEXT("%s"), *OutputJsonString);

				StatsWriteRequest->SetContent(BackendStatsData);
				StatsWriteRequest->ProcessRequest();
			}
		}
	}
}

void AUTPlayerState::StatsWriteComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (HttpRequest.IsValid() && HttpResponse.IsValid())
	{
		if (bSucceeded && HttpResponse->GetResponseCode() == 200)
		{
			UE_LOG(LogGameStats, Verbose, TEXT("Stats write succeeded %s %s"), *HttpRequest->GetURL(), *HttpResponse->GetContentAsString());
		}
		else
		{
			UE_LOG(LogGameStats, Warning, TEXT("Stats write failed %s %s"), *HttpRequest->GetURL(), *HttpResponse->GetContentAsString());
		}
	}
}

void AUTPlayerState::AddMatchToStats(const FString& MapName, const FString& GameType, const TArray<class AUTTeamInfo*>* Teams, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates)
{
	if (StatManager != nullptr && !StatsID.IsEmpty())
	{
		StatManager->AddMatchToStats(MapName, GameType, Teams, ActivePlayerStates, InactivePlayerStates);
	}
}

bool AUTPlayerState::OwnsItemFor(const FString& Path, int32 VariantId) const
{
#if WITH_PROFILE
	if (GetMcpProfile() != NULL)
	{
		TArray<UUTProfileItem*> ItemList;
		GetMcpProfile()->GetItemsOfType<UUTProfileItem>(ItemList);
		for (UUTProfileItem* Item : ItemList)
		{
			if (Item != NULL && Item->Grants(Path, VariantId))
			{
				return true;
			}
		}
	}
#endif
	return false;
}

bool AUTPlayerState::HasRightsFor(UObject* Obj) const
{
	if (Obj == NULL)
	{
		return true;
	}
	else
	{
		IOnlineEntitlementsPtr EntitlementInterface = IOnlineSubsystem::Get()->GetEntitlementsInterface();
		FUniqueEntitlementId Entitlement = GetRequiredEntitlementFromObj(Obj);
		if (EntitlementInterface.IsValid() && !Entitlement.IsEmpty() && !EntitlementInterface->GetItemEntitlement(*UniqueId.GetUniqueNetId().Get(), Entitlement).IsValid())
		{
			return false;
		}
		else
		{
			return !NeedsProfileItem(Obj) || OwnsItemFor(Obj->GetPathName());
		}
	}
}

bool AUTPlayerState::ValidateEntitlementSingleObject(UObject* Object)
{
	if (IOnlineSubsystem::Get() != NULL && UniqueId.IsValid() && !IsProfileItemListPending())
	{
		IOnlineEntitlementsPtr EntitlementInterface = IOnlineSubsystem::Get()->GetEntitlementsInterface();
		if (EntitlementInterface.IsValid())
		{
			// we assume that any successful entitlement query is going to return at least one - the entitlement to play the game
			TArray< TSharedRef<FOnlineEntitlement> > AllEntitlements;
			EntitlementInterface->GetAllEntitlements(*UniqueId.GetUniqueNetId().Get(), TEXT("ut"), AllEntitlements);
			if (AllEntitlements.Num() > 0)
			{
				if (!HasRightsFor(Object))
				{
					return false;
				}
			}
		}
	}

	return true;
}

void AUTPlayerState::ValidateEntitlements()
{
	if (IOnlineSubsystem::Get() != NULL && UniqueId.IsValid() && !IsProfileItemListPending())
	{
		IOnlineEntitlementsPtr EntitlementInterface = IOnlineSubsystem::Get()->GetEntitlementsInterface();
		if (EntitlementInterface.IsValid())
		{
			// we assume that any successful entitlement query is going to return at least one - the entitlement to play the game
			TArray< TSharedRef<FOnlineEntitlement> > AllEntitlements;
			EntitlementInterface->GetAllEntitlements(*UniqueId.GetUniqueNetId().Get(), TEXT("ut"), AllEntitlements);
			if (AllEntitlements.Num() > 0)
			{
				if (!HasRightsFor(HatClass))
				{
					ServerReceiveHatClass(FString());
				}
				if (!HasRightsFor(LeaderHatClass))
				{
					ServerReceiveLeaderHatClass(FString());
				}
				if (!HasRightsFor(EyewearClass))
				{
					ServerReceiveEyewearClass(FString());
				}
				if (!HasRightsFor(GroupTauntClass))
				{
					ServerReceiveGroupTauntClass(FString());
				}
				if (!HasRightsFor(TauntClass))
				{
					ServerReceiveTauntClass(FString());
				}
				if (!HasRightsFor(Taunt2Class))
				{
					ServerReceiveTaunt2Class(FString());
				}
				if (!HasRightsFor(SelectedCharacter))
				{
					ServerSetCharacter(FString());
				}
			}
		}
	}
}

void AUTPlayerState::OnRep_UniqueId()
{
	Super::OnRep_UniqueId();
	// Find the current first local player and asks if I'm their friend.  NOTE: the UTLocalPlayer will, when the friends list changes
	// update these as well.
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(GetWorld(),0));
	if (LP != NULL)
	{
		bIsFriend = LP->IsAFriend(UniqueId);
	}

	ForceUpdatePlayerInfo();
}

void AUTPlayerState::RegisterPlayerWithSession(bool bWasFromInvite)
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver)
	{
		return;
	}

	Super::RegisterPlayerWithSession(bWasFromInvite);
}

void AUTPlayerState::UnregisterPlayerWithSession()
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver)
	{
		return;
	}

	Super::UnregisterPlayerWithSession();
}

#if !UE_SERVER
const FSlateBrush* AUTPlayerState::GetXPStarImage(bool bSmall) const
{
	int32 Star = 0;
	UUTLocalPlayer::GetStarsFromXP(GetLevelForXP(PrevXP), Star);
	if (Star > 0 && Star <= 5)
	{
		FString StarStr = FString::Printf(TEXT("UT.RankStar.%i"), Star-1);
		if (bSmall) StarStr += TEXT(".Small");
		return SUTStyle::Get().GetBrush(*StarStr);
	}

	return SUTStyle::Get().GetBrush("UT.RankStar.Empty");
}

TSharedRef<SWidget> AUTPlayerState::BuildRank(AUTBaseGameMode* DefaultGame, bool bRankedSession, FText RankName)
{
	int32 Badge;
	int32 Level;
	GetBadgeFromELO(DefaultGame, bRankedSession, Badge, Level);

	FText ELOText = (Badge > 0 ?
			FText::Format(NSLOCTEXT("AUTPlayerState", "ELOScoreA", "     ({0})"), FText::AsNumber(DefaultGame ? DefaultGame->GetEloFor(this, bRankedSession) : 0)) :
			FText::Format(NSLOCTEXT("AUTPlayerState", "ELOScoreB", "     Provisional Rating: {0}"), FText::AsNumber(DefaultGame ? DefaultGame->GetEloFor(this, bRankedSession) : 0))
			);

	FText RankNumber = FText::AsNumber(Level+1);

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				SNew(STextBlock)
				.Text(RankName)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(5.0, 0.0, 0.0, 0.0)
		.AutoWidth()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox).WidthOverride(48).HeightOverride(48)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(GetELOBadgeImage(DefaultGame, bRankedSession, true))
					]
					+ SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Padding(FMargin(0.00,0.0,0.0,0.0))
						[
							SNew(STextBlock)
							.Text(RankNumber)
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween.Bold")
							.ShadowOffset(FVector2D(0.0f,2.0f))
							.ShadowColorAndOpacity(FLinearColor(0.0f,0.0f,0.0f,1.0f))

						]
					]
				]
			]
		]
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(500)
			[
				SNew(STextBlock)
				.Text(((DefaultGame->GetNumMatchesFor(this, bRankedSession) > 10) ? ELOText : NSLOCTEXT("Generic", "NotValidELO", "     Less than 10 matches played.")))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		];
}

FText AUTPlayerState::LeagueTierToText(int32 Tier)
{
	switch (Tier)
	{
	case 5:
		return NSLOCTEXT("Generic", "GrandMasterLeague", "Grand Master");
	case 4:
		return NSLOCTEXT("Generic", "MasterLeague", "Master");
	case 3:
		return NSLOCTEXT("Generic", "PlatinumLeague", "Platinum");
	case 2:
		return NSLOCTEXT("Generic", "GoldLeague", "Gold");
	case 1:
		return NSLOCTEXT("Generic", "SilverLeague", "Silver");
	}

	return NSLOCTEXT("Generic", "BronzeLeague", "Bronze");
}

FString AUTPlayerState::LeagueTierToBrushName(int32 Tier)
{
	switch (Tier)
	{
	case 5:
	case 4:
		return L"UT.RankedMaster";
	case 3:
		return L"UT.RankedPlatinum";
	case 2:
		return L"UT.RankedGold";
	case 1:
		return L"UT.RankedSilver";
	}

	return L"UT.RankedBronze";
}

TSharedRef<SWidget> AUTPlayerState::BuildLeague(AUTBaseGameMode* DefaultGame, FText LeagueName)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	UUTLocalPlayer* LP = nullptr;
	FRankedLeagueProgress LeagueProgress;
	if (PC != NULL)
	{
		LP = Cast<UUTLocalPlayer>(PC->Player);
	}

	FText LeagueText = NSLOCTEXT("Generic", "LeagueNoData", "No League Data");
	if (LP)
	{
		if (LP->GetLeagueProgress(DefaultGame->GetRankedLeagueName(), LeagueProgress))
		{
			LeagueText = ((LeagueProgress.LeaguePlacementMatches >= 10) ?
				FText::Format(NSLOCTEXT("AUTPlayerState", "LeagueText", "     {0} {1} ({2})"), LeagueTierToText(LeagueProgress.LeagueTier), FText::AsNumber(LeagueProgress.LeagueDivision + 1), FText::AsNumber(LeagueProgress.LeaguePoints)) :
				FText::Format(NSLOCTEXT("AUTPlayerState", "LeaguePlacementText", "     Play {0} more placement matches"), FText::AsNumber(10 - LeagueProgress.LeaguePlacementMatches))
				);
		}
	}


	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				SNew(STextBlock)
				.Text(LeagueName)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(5.0, 0.0, 0.0, 0.0)
		.AutoWidth()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[				
				SNew(SBox)
				.WidthOverride(48)
				.HeightOverride(48)
				[
					SNew(STextBlock)
					.Text(FText())
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
					.ColorAndOpacity(FLinearColor::Gray)
				]
			]
		]
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(500)
			[
				SNew(STextBlock)
				.Text(LeagueText)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		];
}

TSharedRef<SWidget> AUTPlayerState::BuildLeagueInfo()
{
	TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox);

	VBox->AddSlot()
	.AutoHeight()
	[
		BuildLeagueInfoPane(NAME_RankedShowdownSkillRating.ToString(), NSLOCTEXT("Generic", "ShowdownLeagueHeader", "3v3 Showdown League"))
	];
	
	VBox->AddSlot()
	.Padding(10.0f, 5.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SBox)
		.HeightOverride(2.f)
		[
			SNew(SImage)
			.Image(SUTStyle::Get().GetBrush("UT.Divider.Black"))
		]
	];
	
	VBox->AddSlot()
	.AutoHeight()
	[
		BuildLeagueInfoPane(NAME_RankedCTFSkillRating.ToString(), NSLOCTEXT("Generic", "CTFLeagueHeader", "5v5 CTF League"))
	];
	
	VBox->AddSlot()
	.Padding(10.0f, 5.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SBox)
		.HeightOverride(2.f)
		[
			SNew(SImage)
			.Image(SUTStyle::Get().GetBrush("UT.Divider.Black"))
		]
	];
	
	VBox->AddSlot()
	.AutoHeight()
	[
		BuildLeagueInfoPane(NAME_RankedDuelSkillRating.ToString(), NSLOCTEXT("Generic", "DuelLeagueHeader", "1v1 Duel League"))
	];

	return VBox;
}

TSharedRef<SWidget> AUTPlayerState::BuildLeagueInfoPane(const FString& LeagueType, const FText& LeagueName)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	UUTLocalPlayer* LP = nullptr;
	if (PC != NULL)
	{
		LP = Cast<UUTLocalPlayer>(PC->Player);
	}
	
	TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox);
	
	VBox->AddSlot()
	.Padding(10.0f, 0.0f, 10.0f, 5.0f)
	.AutoHeight()
	[				
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				SNew(STextBlock)
				.Text(LeagueName)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		]
	];
	VBox->AddSlot()
	.Padding(10.0f, 5.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SBox)
		.HeightOverride(2.f)
		[
			SNew(SImage)
			.Image(SUTStyle::Get().GetBrush("UT.Divider"))
		]
	];
	if (LP)
	{
		FRankedLeagueProgress LeagueProgress;
		FMMREntry MMREntry;
		LP->GetMMREntry(LeagueType, MMREntry);
		LP->GetLeagueProgress(LeagueType, LeagueProgress);
		if (LeagueProgress.LeaguePlacementMatches < 10)
		{
			VBox->AddSlot()
			.Padding(10.0f, 0.0f, 10.0f, 5.0f)
			.AutoHeight()
			[
				BuildLeagueDataRow(NSLOCTEXT("Generic", "3v3ShowdownMatchesUntil", "Matches Until Placement :"), FText::AsNumber(10 - LeagueProgress.LeaguePlacementMatches))
			];
			VBox->AddSlot()
			.Padding(10.0f, 0.0f, 10.0f, 5.0f)
			.AutoHeight()
			[
				BuildLeagueDataRow(NSLOCTEXT("Generic", "3v3ShowdownMMR", "MMR :"), FText::AsNumber(MMREntry.MMR))
			];
		}
		else
		{
			VBox->AddSlot()
			.Padding(10.0f, 0.0f, 10.0f, 5.0f)
			.AutoHeight()
			[
				BuildLeagueDivision(LeagueProgress.LeagueTier, LeagueProgress.LeagueDivision)
			];
			
			VBox->AddSlot()
			.Padding(10.0f, 0.0f, 10.0f, 5.0f)
			.AutoHeight()
			[
				BuildLeagueDataRow(NSLOCTEXT("Generic", "3v3ShowdownPoints", "League Points :"), FText::AsNumber(LeagueProgress.LeaguePoints))
			];
			
			VBox->AddSlot()
			.Padding(10.0f, 0.0f, 10.0f, 5.0f)
			.AutoHeight()
			[
				BuildLeagueDataRow(NSLOCTEXT("Generic", "3v3ShowdownMMR", "MMR :"), FText::AsNumber(MMREntry.MMR))
			];
			
			VBox->AddSlot()
			.Padding(10.0f, 0.0f, 10.0f, 5.0f)
			.AutoHeight()
			[
				BuildLeagueDataRow(NSLOCTEXT("Generic", "3v3ShowdownNumMatches", "Matches played :"), FText::AsNumber(MMREntry.MatchesPlayed))
			];

			if (LeagueProgress.bLeaguePromotionSeries)
			{
				int32 NeededPromoSeriesWins = 3;
				if (LeagueProgress.LeagueDivision == 0)
				{
					NeededPromoSeriesWins = 5;
				}
				FText PromoText = FText::Format(NSLOCTEXT("Generic", "3v3ShowdownPromotion", "{0} / {1} (Best of {2})"), FText::AsNumber(LeagueProgress.LeaguePromotionMatchesWon), FText::AsNumber(LeagueProgress.LeaguePromotionMatchesAttempted), FText::AsNumber(NeededPromoSeriesWins));
				
				VBox->AddSlot()
				.Padding(10.0f, 0.0f, 10.0f, 5.0f)
				.AutoHeight()
				[
					BuildLeagueDataRow(NSLOCTEXT("Generic", "3v3ShowdownPromoSeries", "Promotion Series :"), PromoText)
				];
			}
		}
	}
	return VBox;
}

TSharedRef<SWidget> AUTPlayerState::BuildLeagueDivision(int32 Tier, int32 Division)
{
	TSharedRef<SHorizontalBox> HBox = SNew(SHorizontalBox);
	
	HBox->AddSlot()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(SBox)
		.WidthOverride(300)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("Generic", "3v3ShowdownPlacementLabel", "Division :"))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.ColorAndOpacity(FLinearColor::Gray)
		]
	];
	
	FText PlacementText = FText::Format(NSLOCTEXT("Generic", "3v3ShowdownPlacement", "{0} {1}"), LeagueTierToText(Tier), FText::AsNumber(Division + 1));

	HBox->AddSlot()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image(SUTStyle::Get().GetBrush(*LeagueTierToBrushName(Tier)))
			]
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.MaxHeight(128)
				[
					SNew(STextBlock)
					.Text(FText::AsNumber(Division + 1))
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large.Bold")
					.ColorAndOpacity(FLinearColor::White)
				]
			]
		]
		+SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(PlacementText)
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.ColorAndOpacity(FLinearColor::Gray)
		]
	];

	return HBox;
}

TSharedRef<SWidget> AUTPlayerState::BuildLeagueDataRow(FText Label, FText Data)
{
	TSharedRef<SHorizontalBox> HBox = SNew(SHorizontalBox);

	HBox->AddSlot()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(SBox)
		.WidthOverride(300)
		[
			SNew(STextBlock)
			.Text(Label)
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.ColorAndOpacity(FLinearColor::Gray)
		]
	];

	HBox->AddSlot()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	.AutoWidth()
	[
		SNew(SBox)
		.WidthOverride(500)
		[
			SNew(STextBlock)
			.Text(Data)
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.ColorAndOpacity(FLinearColor::Gray)
		]
	];

	return HBox;
}

TSharedRef<SWidget> AUTPlayerState::BuildRankInfo()
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	UUTLocalPlayer* LP = nullptr;
	if (PC != NULL)
	{
		LP = Cast<UUTLocalPlayer>(PC->Player);
	}

	TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox);
	VBox->AddSlot()
	.Padding(10.0f, 5.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SBox)
		.HeightOverride(2.f)
		[
			SNew(SImage)
			.Image(SUTStyle::Get().GetBrush("UT.Divider"))
		]
	];
	VBox->AddSlot()
	.Padding(10.0f, 0.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		BuildRank(AUTShowdownGame::StaticClass()->GetDefaultObject<AUTGameMode>(), false, NSLOCTEXT("Generic", "ShowdownRank", "Showdown Rank :"))
	];
	VBox->AddSlot()
	.Padding(10.0f, 0.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		BuildRank(AUTFlagRunGame::StaticClass()->GetDefaultObject<AUTGameMode>(), false, NSLOCTEXT("Generic", "FlagRunRank", "Flag Run Rank :"))
	];
	VBox->AddSlot()
	.Padding(10.0f, 0.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		BuildRank(AUTCTFBaseGame::StaticClass()->GetDefaultObject<AUTGameMode>(), false, NSLOCTEXT("Generic", "CTFRank", "Capture the Flag Rank :"))
	];
	VBox->AddSlot()
	.Padding(10.0f, 0.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		BuildRank(AUTDuelGame::StaticClass()->GetDefaultObject<AUTGameMode>(), false, NSLOCTEXT("Generic", "DuelRank", "Duel Rank :"))
	];
	VBox->AddSlot()
	.Padding(10.0f, 0.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		BuildRank(AUTTeamDMGameMode::StaticClass()->GetDefaultObject<AUTGameMode>(), false, NSLOCTEXT("Generic", "TDMRank", "Team Deathmatch Rank :"))
	];
	VBox->AddSlot()
	.Padding(10.0f, 0.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		BuildRank(AUTDMGameMode::StaticClass()->GetDefaultObject<AUTGameMode>(), false, NSLOCTEXT("Generic", "DMRank", "Deathmatch Rank :"))
	];
	VBox->AddSlot()
	.Padding(10.0f, 5.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SBox)
		.HeightOverride(2.f)
		[
			SNew(SImage)
			.Image(SUTStyle::Get().GetBrush("UT.Divider"))
		]
	];

#if WITH_PROFILE
	// use profile if available, in case new data was received from MCP since the server set the replicated value
	UUtMcpProfile* Profile = GetMcpProfile();
	if (Profile == NULL && LP != NULL)
	{
		Profile = LP->GetMcpProfileManager()->GetMcpProfileAs<UUtMcpProfile>(EUtMcpProfile::Profile);
	}
	if (Profile != NULL)
	{
		PrevXP = Profile->GetXP();
	}
#endif
	int32 Level = GetLevelForXP(PrevXP);
	int32 LevelXPStart = GetXPForLevel(Level);
	int32 LevelXPEnd = GetXPForLevel(Level + 1);
	int32 LevelXPRange = LevelXPEnd - LevelXPStart;

	FText TooltipXP = FText::Format(NSLOCTEXT("AUTPlayerState", "XPTooltipCap", " {0} XP"), FText::AsNumber(FMath::Max(0, PrevXP)));
	float LevelAlpha = 1.0f;

	if (LevelXPRange > 0)
	{
		LevelAlpha = (float)(PrevXP - LevelXPStart) / (float)LevelXPRange;
		TooltipXP = FText::Format(NSLOCTEXT("AUTPlayerState", "XPTooltip", "{0} XP needed for the next level"), FText::AsNumber(LevelXPEnd - PrevXP));
	}

	VBox->AddSlot()
	.Padding(10.0f, 10.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("AUTPlayerState", "LevelNum", "Experience Level:"))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox).WidthOverride(77).HeightOverride(77)
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::AsNumber(Level))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Tween.Bold")
							.ShadowOffset(FVector2D(0.0f,2.0f))
							.ShadowColorAndOpacity(FLinearColor(0.0f,0.0f,0.0f,1.0f))					
						]
					]
					+SOverlay::Slot()
					[
						SNew(SImage)
						.Image(GetXPStarImage(false))
					]
				]
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::Format(NSLOCTEXT("AUTPlayerState", "LevelFormat", " ({0} XP Total)"), FText::AsNumber(FMath::Max(0, PrevXP))))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.ColorAndOpacity(FLinearColor::Gray)
		]
	];
	VBox->AddSlot()
	.Padding(10.0f, 10.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("AUTPlayerState", "Progress", "Progress to next level:"))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::AsNumber(LevelXPStart))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.ColorAndOpacity(FLinearColor::Gray)
		]
		+ SHorizontalBox::Slot()
		.Padding(25.0, 0.0, 25.0, 0.0)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(500.0f).HeightOverride(14.0f)
			[
				SNew(SProgressBar)
				.Style(SUTStyle::Get(),"UT.ProgressBar")
				.Percent(LevelAlpha)
				.ToolTipText(TooltipXP)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::AsNumber(LevelXPEnd))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.ColorAndOpacity(FLinearColor::Gray)
		]

	];

	if (LP != NULL)
	{
		VBox->AddSlot()
			.Padding(10.0f, 10.0f, 10.0f, 5.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(300)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("AUTPlayerState", "ChallengeStars", "Challenge Stars:"))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.ColorAndOpacity(FLinearColor::Gray)
			]
			]
		+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::Format(NSLOCTEXT("AUTPlayerState", "ChallengeStarsFormat", "{0} "), FText::AsNumber(LP->GetRewardStars(NAME_REWARD_GoldStars))))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.ColorAndOpacity(FLinearColor::Gray)
			]
		+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
			[
				SNew(SBox).WidthOverride(32).HeightOverride(32)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.Star"))
					.ColorAndOpacity(FLinearColor::Yellow)
				]
			]
			]
		+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::Format(NSLOCTEXT("AUTPlayerState", "ChallengeStarsFormatBlue", ".          {0} "), FText::AsNumber(LP->GetRewardStars(NAME_REWARD_BlueStars))))
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
					.ColorAndOpacity(FLinearColor::Gray)
			]
		+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
			[
				SNew(SBox).WidthOverride(32).HeightOverride(32)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.Star"))
			.ColorAndOpacity(FLinearColor::Blue)
				]
			]
			]
			];
	}
	else
	{
		VBox->AddSlot()
			.Padding(10.0f, 10.0f, 10.0f, 5.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(300)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("AUTPlayerState", "ChallengeStars", "Challenge Stars:"))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.ColorAndOpacity(FLinearColor::Gray)
			]
			]
		+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::Format(NSLOCTEXT("AUTPlayerState", "ChallengeStarsFormat", "{0} "), FText::AsNumber(TotalChallengeStars)))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.ColorAndOpacity(FLinearColor::Gray)
			]
		+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
			[
				SNew(SBox).WidthOverride(32).HeightOverride(32)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.Star"))
				]
			]
			]
			];
	}
	return VBox;
}

TSharedRef<SWidget> AUTPlayerState::BuildStatsInfo()
{
	TSharedRef<SHorizontalBox> HBox = SNew(SHorizontalBox);
	if (!StatsID.IsEmpty())
	{
		HBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Generic", "EpicIDPrompt", "ID :"))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		];
		HBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(5.0, 0.0, 0.0, 0.0)
		.AutoWidth()
		[
			SNew(SHyperlink)
			.Text(FText::FromString(StatsID))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.OnNavigate(FSimpleDelegate::CreateUObject(this, &AUTPlayerState::EpicIDClicked))
		];
	}

	return HBox;
}

void AUTPlayerState::BuildPlayerInfo(TSharedPtr<SUTTabWidget> TabWidget, TArray<TSharedPtr<TAttributeStat> >& StatList)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(GetOwner());
	UUTLocalPlayer* LP = PC ? Cast<UUTLocalPlayer>(PC->Player) : NULL;

	if (LP && StatsID.IsEmpty() && OnlineIdentityInterface.IsValid() && OnlineIdentityInterface->GetLoginStatus(LP->GetControllerId()))
	{
		TSharedPtr<const FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(LP->GetControllerId());
		if (UserId.IsValid())
		{
			StatsID = UserId->ToString();
		}
	}

	FText EpicAccountName = FText::GetEmpty();
	if (!StatsID.IsEmpty())
	{
		TSharedRef<const FUniqueNetId> UserId = MakeShareable(new FUniqueNetIdString(*StatsID));
		if (UserId->IsValid())
		{	
			if (GetWorld())
			{
				AUTGameState* UTGS = GetWorld()->GetGameState<AUTGameState>();
				if (UTGS != nullptr)
				{
					EpicAccountName = UTGS->GetEpicAccountNameForAccount(UserId);
				}
			}
		}
	}

	UUTFlagInfo* Flag = Cast<UUTGameEngine>(GEngine) ? Cast<UUTGameEngine>(GEngine)->GetFlag(CountryFlag) : nullptr;
	if ((Avatar == NAME_None) && PC)
	{
		if (LP)
		{
			Avatar = LP->GetAvatar();
		}
	}
	TabWidget->AddTab(NSLOCTEXT("AUTPlayerState", "PlayerInfo", "Player Info"),
	SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.Padding(10.0f, 20.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			[
				SNew(STextBlock)
				.Text(EpicAccountName)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			]
		]
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			//blank space between name and player icon
			SNew(SBox)
			.WidthOverride(64.0f)
			.HeightOverride(64.0f)
			.MaxDesiredWidth(64.0f)
			.MaxDesiredHeight(64.0f)
			[
				SNew(SImage)
				.Image(SUTStyle::Get().GetBrush("UT.NoStyle"))
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(64.0f)
			.HeightOverride(64.0f)
			.MaxDesiredWidth(64.0f)
			.MaxDesiredHeight(64.0f)
			[
				SNew(SImage)
				.Image((Avatar != NAME_None) ? SUTStyle::Get().GetBrush(Avatar) : SUTStyle::Get().GetBrush("UT.NoStyle"))
			]
		]
	]
	+ SVerticalBox::Slot()
	.Padding(10.0f, 20.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(36.0f)
			.HeightOverride(26.0f)
			.MaxDesiredWidth(36.0f)
			.MaxDesiredHeight(26.0f)
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush(Flag ? Flag->GetSlatePropertyName() : NAME_None))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(10.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Flag ? Flag->GetFriendlyName() : TEXT("")))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
		]
	]
	+ SVerticalBox::Slot()
	.FillHeight(1.0)
	[
		BuildRankInfo()
	]
	+SVerticalBox::Slot()
	.Padding(10.0f, 20.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		BuildStatsInfo()
	]
	
	+ SVerticalBox::Slot()
	.Padding(10.0f, 20.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		BuildSeasonInfo()
	]
	);

	// Would be great if this worked on remote players
	if (LP)
	{
		TabWidget->AddTab(NSLOCTEXT("AUTPlayerState", "LeagueInfo", "League Info"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(10.0f, 20.0f, 10.0f, 5.0f)
		.AutoHeight()
		[
			BuildLeagueInfo()
		]);
	}
}

TSharedRef<SWidget> AUTPlayerState::BuildSeasonInfo()
{
	TSharedRef<SVerticalBox> VBox = SNew(SVerticalBox);

	AUTPlayerController* PC = Cast<AUTPlayerController>(GetOwner());
	UUTLocalPlayer* LP = PC ? Cast<UUTLocalPlayer>(PC->Player) : NULL;

	static FName FacePumpkins(TEXT("FacePumpkins"));
	float TotalPumpkins = 0.0f;
	UUTGameplayStatics::GetBestTime(GetWorld(), FacePumpkins, TotalPumpkins);

	if (GetWorld() && LP)
	{
		VBox->AddSlot()
		.Padding(10.0f, 5.0f, 10.0f, 5.0f)
		[
			SNew(SBox)
			.HeightOverride(2.f)
			[
				SNew(SImage)
				.Image(SUTStyle::Get().GetBrush("UT.Divider"))
			]
		];
		VBox->AddSlot()
		.Padding(10.0f, 10.0f, 10.0f, 5.0f)
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(EVerticalAlignment::VAlign_Top)
				.HAlign(EHorizontalAlignment::HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText(NSLOCTEXT("AUTPlayerState", "Seasons", "2016 Pre-Alpha Seasonal Stats")))
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				]
			]
			+ SVerticalBox::Slot()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(10.0f, 10.0f, 10.0f, 5.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(EHorizontalAlignment::HAlign_Left)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::Format(NSLOCTEXT("Halloween", "PumpkinsCollected", "Halloween Pumpkins Collected:   {0}"), FText::AsNumber(TotalPumpkins)))
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
					.ColorAndOpacity(FLinearColor::Gray)
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					//blank space between name and player icon
					SNew(SBox)
					.WidthOverride(12.0)
					.HeightOverride(64.0f)
					.MaxDesiredWidth(12.0f)
					.MaxDesiredHeight(64.0f)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush("UT.NoStyle"))
					]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(45.f)
					.HeightOverride(45.f)
					.MaxDesiredWidth(45.f)
					.MaxDesiredHeight(45.f)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush("UT.Icon.Pumpkin"))
					]
				]
			]
		];
	}

	return VBox;
}

FText AUTPlayerState::GetTrainingLevelText()
{
	return FText::FromString(TEXT("Untested"));
	//return FText::FromString(TEXT("Incomplete"));
	//return FText::FromString(TEXT("Competent"));
	//return FText::FromString(TEXT("Mastered"));
	//return FText::FromString(TEXT("Expert"));
	//return FText::FromString(TEXT("Pro"));
}

void AUTPlayerState::EpicIDClicked()
{
	FPlatformMisc::ClipboardCopy(*StatsID);
}
#endif

void AUTPlayerState::UpdateOldName()
{
	OldName = PlayerName;
}

void AUTPlayerState::OnRep_PlayerName()
{
	bHasValidClampedName = false;

	if (PlayerName.IsEmpty())
	{
		// Demo rec spectator is allowed empty name
		return;
	}

	if (GetWorld()->TimeSeconds < 2.f)
	{
		OldName = PlayerName;
		bHasBeenWelcomed = true;
		return;
	}

	if (!GetWorldTimerManager().IsTimerActive(UpdateOldNameHandle))
	{
		GetWorldTimerManager().SetTimer(UpdateOldNameHandle, this, &AUTPlayerState::UpdateOldName, 5.f, false);
	}

	// new player or name change
	if (bHasBeenWelcomed)
	{
		if (ShouldBroadCastWelcomeMessage())
		{
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController* PlayerController = Iterator->Get();
				if (PlayerController != NULL && PlayerController->IsLocalPlayerController())
				{
					PlayerController->ClientReceiveLocalizedMessage(EngineMessageClass, 2, this);
				}
			}
		}
	}
	else
	{
		int32 WelcomeMessageNum = bOnlySpectator ? 16 : 1;
		bHasBeenWelcomed = true;

		if (ShouldBroadCastWelcomeMessage())
		{
			for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController* PlayerController = Iterator->Get();
				if (PlayerController != NULL && PlayerController->IsLocalPlayerController())
				{
					PlayerController->ClientReceiveLocalizedMessage(EngineMessageClass, WelcomeMessageNum, this);
				}
			}
		}
	}
}


void AUTPlayerState::SetOverrideHatClass(const FString& NewOverrideHatClass)
{
	OverrideHatClass = NewOverrideHatClass == TEXT("") ? nullptr : LoadClass<AUTHat>(NULL, *NewOverrideHatClass, NULL, GetCosmeticLoadFlags(), NULL);
	OnRepOverrideHat();
}

void AUTPlayerState::OnRepOverrideHat()
{
	AUTCharacter* UTChar = GetUTCharacter();
	UE_LOG(UT,Log,TEXT("OnRepOverrideHat: %s %s"), (OverrideHatClass == nullptr ? TEXT("None") : *OverrideHatClass->GetFullName()), ( UTChar ? *UTChar->GetFullName() : TEXT("None") ) );
	if (UTChar != nullptr)
	{
		UTChar->SetHatClass(OverrideHatClass == nullptr ? HatClass : OverrideHatClass);
	}
}

bool AUTPlayerState::ServerUpdateLoadout_Validate(const TArray<AUTReplicatedLoadoutInfo*>& NewLoadout) { return true; }
void AUTPlayerState::ServerUpdateLoadout_Implementation(const TArray<AUTReplicatedLoadoutInfo*>& NewLoadout)
{
	Loadout = NewLoadout;
}

bool AUTPlayerState::ServerBuyLoadout_Validate(AUTReplicatedLoadoutInfo* DesiredLoadout) { return true; }
void AUTPlayerState::ServerBuyLoadout_Implementation(AUTReplicatedLoadoutInfo* DesiredLoadout)
{
	AUTCharacter* UTChar = GetUTCharacter();
	if (UTChar != nullptr)
	{
		if (DesiredLoadout->CurrentCost <= GetAvailableCurrency())
		{
				UTChar->AddInventory(GetWorld()->SpawnActor<AUTInventory>(DesiredLoadout->ItemClass, FVector(0.0f), FRotator(0, 0, 0)), true);
				AdjustCurrency(DesiredLoadout->CurrentCost * -1);
		}
		else
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(GetOwner());	
			if (PC)
			{
				PC->ClientReceiveLocalizedMessage(UUTGameMessage::StaticClass(),14, this);
			}
		}
	}
}

void AUTPlayerState::AdjustCurrency(float Adjustment)
{
	AvailableCurrency += Adjustment;
	if (AvailableCurrency < 0.0) AvailableCurrency = 0.0f;
}

float AUTPlayerState::GetAvailableCurrency()
{
	return AvailableCurrency;
}

void AUTPlayerState::ClientShowLoadoutMenu_Implementation()
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(GetOwner());
	if ( PC && PC->Player && Cast<UUTLocalPlayer>(PC->Player) )
	{
		Cast<UUTLocalPlayer>(PC->Player)->OpenLoadout();
	}
}

void AUTPlayerState::SetReadyToPlay(bool bNewReadyState)
{
	bReadyToPlay = bNewReadyState;

	uint8 NewReadyState = bReadyToPlay + (bPendingTeamSwitch >> 2);
	if ((ReadySwitchCount > 2) && bReadyToPlay)
	{
		if (GetWorld()->GetTimeSeconds() - LastReadySwitchTime > 0.2f)
		{
			ReadySwitchCount++;
			LastReadySwitchTime = GetWorld()->GetTimeSeconds();
			ReadyMode = 1;
		}
		if (ReadySwitchCount > 10)
		{
			ReadyMode = 2;
		}
	}
	else if (!bReadyToPlay && (GetWorld()->GetTimeSeconds() - LastReadySwitchTime > 0.5f))
	{
		ReadyMode = 0;
	}
	else if (NewReadyState != LastReadyState)
	{
		ReadySwitchCount = (GetWorld()->GetTimeSeconds() - LastReadySwitchTime < 0.5f) ? ReadySwitchCount + 1 : 0;
		LastReadySwitchTime = GetWorld()->GetTimeSeconds();
	}
	LastReadyState = NewReadyState;
}

void AUTPlayerState::LogBanRequest(AUTPlayerState* Voter)
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState->bDisableVoteKick || (UTGameState->bOnlyTeamCanVoteKick && GetTeamNum() != Voter->GetTeamNum())) 
	{
		return;
	}

	float CurrentTime = GetWorld()->GetRealTimeSeconds();
	for (int32 i=0; i < BanVotes.Num(); i++)
	{
		if (BanVotes[i].Voter->ToString() == Voter->UniqueId.GetUniqueNetId()->ToString())
		{
			if (BanVotes[i].BanTime < CurrentTime - 120.0)
			{
				// Update the ban
				BanVotes[i].BanTime = CurrentTime;
			}
			return;
		}
	}

	BanVotes.Add(FTempBanInfo(Voter->UniqueId.GetUniqueNetId(), CurrentTime));
}

int32 AUTPlayerState::CountBanVotes()
{
	int32 VoteCount = 0;
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		float CurrentTime = GetWorld()->GetRealTimeSeconds();
		int32 Idx = 0;
		while (Idx < BanVotes.Num())
		{
			if (BanVotes[Idx].BanTime < CurrentTime - 120.0)
			{
				// Expired, delete it
				BanVotes.RemoveAt(Idx);
			}
			else
			{
				// only count bans of people online
				for (int32 i=0; i < GameState->PlayerArray.Num(); i++)
				{
					if (GameState->PlayerArray[i]->UniqueId.GetUniqueNetId() == BanVotes[Idx].Voter)
					{
						VoteCount++;
						break;
					}
				}
				Idx++;
			}
		}
	}
	
	return VoteCount;
}

void AUTPlayerState::OnRepSpecialPlayer()
{
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetWorld()->GetFirstPlayerController());
	AUTCharacter* Character = GetUTCharacter();
	if (Character != NULL && !Character->bTearOff)
	{
		UpdateSpecialTacComFor(Character, UTPC);
	}
}

void AUTPlayerState::OnRepSpecialTeamPlayer()
{
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetWorld()->GetFirstPlayerController());
	AUTCharacter* Character = GetUTCharacter();
	if (Character != NULL && !Character->bTearOff)
	{
		UpdateSpecialTacComFor(Character, UTPC);
	}
}

void AUTPlayerState::UpdateSpecialTacComFor(AUTCharacter* Character, AUTPlayerController* UTPC)
{
	Character->SetOutlineLocal(bSpecialPlayer || (bSpecialTeamPlayer && UTPC && UTPC->GetTeamNum() == GetTeamNum()), true);
}

float AUTPlayerState::GetStatsValue(FName StatsName) const
{
	return StatsData.FindRef(StatsName);
}

void AUTPlayerState::SetStatsValue(FName StatsName, float NewValue)
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState && UTGameState->HasMatchStarted())
	{
		LastScoreStatsUpdateTime = GetWorld()->GetTimeSeconds();
		StatsData.Add(StatsName, NewValue);
	}
}

void AUTPlayerState::ModifyStatsValue(FName StatsName, float Change)
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState && UTGameState->HasMatchStarted())
	{
		LastScoreStatsUpdateTime = GetWorld()->GetTimeSeconds();
		float CurrentValue = StatsData.FindRef(StatsName);
		StatsData.Add(StatsName, CurrentValue + Change);
	}
}

bool AUTPlayerState::RegisterVote_Validate(AUTReplicatedMapInfo* VoteInfo) { return true; }
void AUTPlayerState::RegisterVote_Implementation(AUTReplicatedMapInfo* VoteInfo)
{
	if (bOnlySpectator)
	{
		// spectators can't vote
		return;
	}
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	bHasVoted = true;
	if (UTGameState != NULL)
	{
		for (int32 i = 0; i < UTGameState->MapVoteList.Num(); i++)
		{
			if (UTGameState->MapVoteList[i] != NULL)
			{
				UTGameState->MapVoteList[i]->UnregisterVoter(this);
			}
		}

		if (VoteInfo != NULL)
		{
			VoteInfo->RegisterVoter(this);
		}
	}
}

void AUTPlayerState::OnRep_bIsInactive()
{
	//Now that we replicate InactivePRI's the super function is unsafe without these checks
	if (GetWorld() && GetWorld()->GetGameState())
	{
		Super::OnRep_bIsInactive();
	}
}

bool AUTPlayerState::AllowFreezingTaunts() const
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();		
	if (GS == nullptr)
	{
		return false;
	}

	return !GS->IsMatchInProgress() || GS->IsMatchIntermission();
}

bool AUTPlayerState::ServerFasterEmote_Validate()
{
	return true;
}

void AUTPlayerState::ServerFasterEmote_Implementation()
{
	EmoteSpeed = FMath::Min(EmoteSpeed + 0.25f, 3.0f);
	ForceNetUpdate();
	OnRepEmoteSpeed();
}

bool AUTPlayerState::ServerSlowerEmote_Validate()
{
	return true;
}

void AUTPlayerState::ServerSlowerEmote_Implementation()
{
	EmoteSpeed = FMath::Max(EmoteSpeed - 0.25f, AllowFreezingTaunts() ? 0.0f : 0.25f);
	ForceNetUpdate();
	OnRepEmoteSpeed();
}

bool AUTPlayerState::ServerSetEmoteSpeed_Validate(float NewEmoteSpeed)
{
	return true;
}

void AUTPlayerState::ServerSetEmoteSpeed_Implementation(float NewEmoteSpeed)
{
	EmoteSpeed = FMath::Clamp(NewEmoteSpeed, AllowFreezingTaunts() ? 0.0f : 0.25f, 3.0f);
	ForceNetUpdate();
	OnRepEmoteSpeed();
}

void AUTPlayerState::OnRepEmoteSpeed()
{
	AUTCharacter* UTC = GetUTCharacter();
	if (UTC != nullptr)
	{
		UTC->SetEmoteSpeed(EmoteSpeed);
	}
#if !UE_SERVER
	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(GetWorld(), 0));
	if (FirstPlayer != nullptr)
	{
		FirstPlayer->OnEmoteSpeedChanged(this, EmoteSpeed);
	}
#endif
}

void AUTPlayerState::OnRepTaunt()
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver && DemoDriver->IsFastForwarding())
	{
		return;
	}

	if (GetWorld()->GetNetMode() == NM_Client)
	{
		if (DemoDriver && DemoDriver->IsServer())
		{
			AUTDemoRecSpectator* DemoRecSpec = Cast<AUTDemoRecSpectator>(DemoDriver->SpectatorController);
			if (DemoRecSpec && (GetWorld()->GetTimeSeconds() - DemoRecSpec->LastKillcamSeekTime) < 1.0f)
			{
				return;
			}
		}
	}

	if (EmoteReplicationInfo.EmoteCount > 0)
	{
		PlayTauntByIndex(EmoteReplicationInfo.EmoteIndex);
	}
	else
	{
		//Clear any active taunts
		AUTCharacter* UTChar = GetUTCharacter();
		if (UTChar && UTChar->CurrentTaunt && UTChar->GetMesh())
		{
			UAnimInstance* AnimInstance = UTChar->GetMesh()->GetAnimInstance();
			if (AnimInstance != nullptr)
			{
				AnimInstance->Montage_Stop(0.0f, UTChar->CurrentTaunt);
			}
		}
	}
}

void AUTPlayerState::PlayTauntByIndex(int32 TauntIndex)
{
	// SetEmoteSpeed here to make sure that it gets unfrozen if freezing isn't allowed
	if (Role == ROLE_Authority)
	{
		ServerSetEmoteSpeed(EmoteSpeed);
	}

	if (TauntIndex == 0 && TauntClass != nullptr)
	{
		EmoteReplicationInfo.EmoteIndex = TauntIndex;
		PlayTauntByClass(TauntClass);
	}
	else if (TauntIndex == 1 && Taunt2Class != nullptr)
	{
		EmoteReplicationInfo.EmoteIndex = TauntIndex;
		PlayTauntByClass(Taunt2Class);
	}
}

void AUTPlayerState::OnRep_ActiveGroupTaunt()
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver && DemoDriver->IsFastForwarding())
	{
		return;
	}

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && GS->ScoringPlayerState == this && ActiveGroupTaunt != nullptr)
	{
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			AUTCharacter *UTChar = Cast<AUTCharacter>(*It);
			if (UTChar != NULL)
			{
				if (ActiveGroupTaunt->GetDefaultObject<AUTGroupTaunt>()->bCascading)
				{
					if (UTChar->PlayerState == this)
					{
						UTChar->PlayGroupTaunt(ActiveGroupTaunt);
					}
				}
				else
				{
					UTChar->PlayGroupTaunt(ActiveGroupTaunt);
				}
			}
		}

		if (ActiveGroupTaunt->GetDefaultObject<AUTGroupTaunt>()->BGMusic != nullptr)
		{
			UGameplayStatics::PlaySound2D(GetWorld(), ActiveGroupTaunt->GetDefaultObject<AUTGroupTaunt>()->BGMusic);
		}
	}
}

void AUTPlayerState::PlayGroupTaunt()
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState && UTGameState->LineUpHelper && UTGameState->LineUpHelper->bIsActive)
	{
		// SetEmoteSpeed here to make sure that it gets unfrozen if freezing isn't allowed
		if (Role == ROLE_Authority)
		{
			ServerSetEmoteSpeed(EmoteSpeed);
		}

		ActiveGroupTaunt = GroupTauntClass;
		OnRep_ActiveGroupTaunt();
	}
}

void AUTPlayerState::PlayTauntByClass(TSubclassOf<AUTTaunt> TauntToPlay)
{
	if (TauntToPlay != nullptr && TauntToPlay->GetDefaultObject<AUTTaunt>()->TauntMontage)
	{
		if (Role == ROLE_Authority)
		{
			if (EmoteReplicationInfo.EmoteCount == 0)
			{
				AddCoolFactorEvent(50.0f);
			}

			EmoteReplicationInfo.EmoteCount++;
			ForceNetUpdate();
		}

		//Play taunt on the character in the game world
		AUTCharacter* UTC = GetUTCharacter();
		if (UTC != nullptr)
		{
			UTC->PlayTauntByClass(TauntToPlay, EmoteSpeed);
		}
		//Pass the taunt along to characters in the MatchSummary
#if !UE_SERVER
		UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(GetWorld(), 0));
		if (FirstPlayer != nullptr)
		{
			FirstPlayer->OnTauntPlayed(this, TauntToPlay, EmoteSpeed);
		}
#endif
	}
}

void AUTPlayerState::ClientReceiveRconMessage_Implementation(const FString& Message)
{	
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(GetOwner());
	if (PC)
	{
		PC->ShowAdminMessage(Message);
	}
}

bool AUTPlayerState::IsABeginner(AUTBaseGameMode* DefaultGameMode) const
{
	int32 Badge = 0;
	int32 Level = 0;
	GetBadgeFromELO(DefaultGameMode, false, Badge, Level);
	return (Badge == 0);
}

int32 AUTPlayerState::GetRankCheck(AUTBaseGameMode* DefaultGameMode)
{
	int32 Badge = 0;
	int32 Level = 0;
	GetBadgeFromELO(DefaultGameMode, false, Badge, Level);
	return AUTPlayerState::CalcRankCheck(Badge,Level);
}

void AUTPlayerState::GetBadgeFromELO(AUTBaseGameMode* DefaultGameMode, bool bRankedSession, int32& BadgeLevel, int32& SubLevel) const
{
	if (DefaultGameMode == nullptr)
	{
		DefaultGameMode = AUTBaseGameMode::StaticClass()->GetDefaultObject<AUTBaseGameMode>();
	}
	int32 NumMatches = DefaultGameMode->GetNumMatchesFor((AUTPlayerState *)this, bRankedSession); // note this is replicated to client as a byte, so max value of 255
	int32 EloRating = DefaultGameMode->GetEloFor((AUTPlayerState *)this, bRankedSession);
	BadgeLevel = 0;
	SubLevel = 0;

	// Elo bounds for bronze levels
	int32 EloBounds[9] = { 670, 900, 1080, 1260, 1360, 1430, 1480, 1510, 1540 };
	if (NumMatches < 20)
	{
		// possibly beginner badges
		if (NumMatches < 3)
		{
			SubLevel = 0;
			return;
		}
		else if (NumMatches < 6)
		{
			SubLevel = 1;
			return;
		}
		else
		{
			int32 i = 0;
			for (i = 0; i < 9; i++)
			{
				if (EloRating < EloBounds[i])
				{
					break;
				}
			}
			int32 MaxLevel = (NumMatches < 10) ? NumMatches / 2 : 8;
			SubLevel = FMath::Clamp(i, 2, MaxLevel);
			return;
		}
	}

	if (EloRating < EloBounds[8])
	{
		int32 i = 0;
		for (i = 0; i < 9; i++)
		{
			if (EloRating < EloBounds[i])
			{
				break;
			}
		}
		BadgeLevel = 1;
		SubLevel = i;
	}
	else if (EloRating < 1900)
	{
		BadgeLevel = 2;
		SubLevel = FMath::Clamp((float(EloRating) - 1540.f) / 40.f, 0.f, 8.f);
	}
	else
	{
		BadgeLevel = 3;
		SubLevel = FMath::Clamp((float(EloRating) - 1900.f) / 50.f, 0.f, 8.f);
	}
}

#if !UE_SERVER
const FSlateBrush* AUTPlayerState::GetELOBadgeImage(AUTBaseGameMode* DefaultGame, bool bRankedSession, bool bSmall) const
{
	int32 Badge = 0;
	int32 Level = 0;

	GetBadgeFromELO(DefaultGame, bRankedSession, Badge, Level);
	FString BadgeStr = FString::Printf(TEXT("UT.RankBadge.%i"), Badge);
	if (bSmall) BadgeStr += TEXT(".Small");
	return SUTStyle::Get().GetBrush(*BadgeStr);
}

const FSlateBrush* AUTPlayerState::GetELOBadgeNumberImage(AUTBaseGameMode* DefaultGame, bool bRankedSession) const
{
	int32 Badge = 0;
	int32 Level = 0;

	GetBadgeFromELO(DefaultGame, bRankedSession, Badge, Level);
	FString BadgeNumberStr = FString::Printf(TEXT("UT.Badge.Numbers.%i"), FMath::Clamp<int32>(Level + 1, 1, 9));
	return SUWindowsStyle::Get().GetBrush(*BadgeNumberStr);
}
#endif



void AUTPlayerState::MakeJsonReport(TSharedPtr<FJsonObject> JsonObject)
{
	JsonObject->SetStringField(TEXT("PlayerName"), PlayerName);
	JsonObject->SetStringField(TEXT("UniqueId"), UniqueId.ToString());
	JsonObject->SetNumberField(TEXT("Score"), Score);
	JsonObject->SetNumberField(TEXT("Team Num"), GetTeamNum());

	AUTBaseGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
	if (GameMode)
	{
		JsonObject->SetNumberField(TEXT("RankCheck"), GetRankCheck(GameMode));
	}

	JsonObject->SetNumberField(TEXT("Duel Rank"), DuelRank);
	JsonObject->SetNumberField(TEXT("No_of_Duel_Played"), DuelMatchesPlayed);
	JsonObject->SetNumberField(TEXT("CTF Rank"), CTFRank);
	JsonObject->SetNumberField(TEXT("No_CTF_Matches Played"), CTFMatchesPlayed);
	JsonObject->SetNumberField(TEXT("TDM Rank"), TDMRank);
	JsonObject->SetNumberField(TEXT("No_TDM_Matches Played"), TDMMatchesPlayed);
	JsonObject->SetNumberField(TEXT("DMRank"), DMRank);
	JsonObject->SetNumberField(TEXT("No_DM_Matches_Played"), DMMatchesPlayed);
	JsonObject->SetNumberField(TEXT("ShowdownRank"), ShowdownRank);
	JsonObject->SetNumberField(TEXT("No_Showdowns"), ShowdownMatchesPlayed);
	JsonObject->SetNumberField(TEXT("FlagRunRank"), FlagRunRank);
	JsonObject->SetNumberField(TEXT("No_FlagRun_Matches_Played"), FlagRunMatchesPlayed);
}

bool AUTPlayerState::ServerSetLoadoutPack_Validate(const FName& NewLoadoutPackTag) { return true; }
void AUTPlayerState::ServerSetLoadoutPack_Implementation(const FName& NewLoadoutPackTag)
{
	if (NewLoadoutPackTag != CurrentLoadoutPackTag)
	{
		AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (GameMode)
		{
			if (GameMode->LoadoutPackIsValid(NewLoadoutPackTag) != INDEX_NONE)
			{
				CurrentLoadoutPackTag = NewLoadoutPackTag;
			}
		}
	}
}


bool AUTPlayerState::ServerUnlockItem_Validate(FName ItemTag, bool bSecondary) { return true; }
void AUTPlayerState::ServerUnlockItem_Implementation(FName ItemTag, bool bSecondary)
{
	if (UnlockList.Find(ItemTag) == INDEX_NONE)
	{
		AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
		if (UTGameState)
		{
			AUTReplicatedLoadoutInfo* UnlockLoadout = UTGameState->FindLoadoutItem(ItemTag);
			if (UnlockLoadout)
			{
				AdjustCurrency(UnlockLoadout->CurrentCost * -1);
				UnlockList.Add(ItemTag);
				ServerSelectLoadout(ItemTag, bSecondary);
				if (GetWorld()->GetNetMode() != NM_DedicatedServer)
				{
					OnUnlockList();
				}
			}
		}
	}

	for (int32 i=0; i < UnlockList.Num() ; i++)
	{
		UE_LOG(UT,Log,TEXT("Unlock: %s"), *UnlockList[i].ToString());
	}
}

bool AUTPlayerState::ServerSelectLoadout_Validate(FName ItemTag, bool bSecondary) { return true; }
void AUTPlayerState::ServerSelectLoadout_Implementation(FName ItemTag, bool bSecondary)
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState)
	{
		AUTReplicatedLoadoutInfo* RepLoadout = UTGameState->FindLoadoutItem(ItemTag);
		if (RepLoadout)
		{
			if (bSecondary)
			{
				SecondarySpawnInventory = RepLoadout;
			}
			else
			{
				PrimarySpawnInventory = RepLoadout;
			}
		}
	}

}

void AUTPlayerState::OnUnlockList()
{
	// Need a better way but until I'm sure this is what we want this will do.
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState)
	{
		for (int32 j=0; j < UTGameState->AvailableLoadout.Num();j++)
		{
			UTGameState->AvailableLoadout[j]->bUnlocked = false;
		}

		for (int32 i=0; i < UnlockList.Num(); i++)
		{
			for (int32 k = 0; k < UTGameState->AvailableLoadout.Num(); k++)
			{
				if (UTGameState->AvailableLoadout[k]->ItemTag == UnlockList[i])
				{
					UTGameState->AvailableLoadout[k]->bUnlocked = true;
				}
			}
		}
	}
}

bool AUTPlayerState::ServerSetBoostItem_Validate(int PowerupIndex) { return true; }
void AUTPlayerState::ServerSetBoostItem_Implementation(int PowerupIndex)
{
	AUTFlagRunGame* FlagRunGame = GetWorld()->GetAuthGameMode<AUTFlagRunGame>();
	if (FlagRunGame && !FlagRunGame->bAllowBoosts)
	{
		BoostClass = nullptr;
		return;
	}
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState)
	{
		TSubclassOf<class AUTInventory> SelectedBoost = UTGameState->GetSelectableBoostByIndex(this, PowerupIndex);
		if (SelectedBoost != nullptr)
		{
			BoostClass = SelectedBoost;
		}
	}
}

TSubclassOf<UUTCharacterVoice> AUTPlayerState::GetCharacterVoiceClass()
{
	if (CharacterVoice == nullptr)
	{
		if (SelectedCharacter == NULL)
		{
			SelectedCharacter = GetDefault<AUTCharacter>()->CharacterData;
		}
		if (SelectedCharacter != nullptr)
		{
			AUTCharacterContent* CharacterDefaultObject = Cast<AUTCharacterContent>(SelectedCharacter.GetDefaultObject());
			if (CharacterDefaultObject != nullptr && CharacterDefaultObject->CharacterVoice != nullptr)
			{
				CharacterVoice = CharacterDefaultObject->CharacterVoice;
			}
		}
	}

	return CharacterVoice;
}

void AUTPlayerState::AddCoolFactorMinorEvent()
{
	AddCoolFactorEvent(50.0f);
}

void AUTPlayerState::AddCoolFactorEvent(float CoolFactorAddition)
{
	// No negative cool please
	if (CoolFactorAddition <= 0)
	{
		return;
	}

	float CurrentWorldTime = GetWorld()->GetTimeSeconds();
	CurrentCoolFactor += CoolFactorAddition;

	if (CurrentCoolFactor >= MinimumConsiderationForCoolFactorHistory)
	{
		UE_LOG(UT, Verbose, TEXT("CoolFactorHistory updated for %s at %f"), *PlayerName, CurrentWorldTime);

		if (CoolFactorHistory.Num() > 0 && CoolFactorHistory[CoolFactorHistory.Num() - 1].TimeOccurred - CurrentWorldTime < CoolFactorCombinationWindow)
		{
			// Overwrite the old cool history if we're cooler, highly likely
			if (CurrentCoolFactor > CoolFactorHistory[CoolFactorHistory.Num() - 1].CoolFactorAmount)
			{
				CoolFactorHistory[CoolFactorHistory.Num() - 1].CoolFactorAmount = CurrentCoolFactor;
				CoolFactorHistory[CoolFactorHistory.Num() - 1].TimeOccurred = CurrentWorldTime;
			}
		}
		else
		{
			FCoolFactorHistoricalEvent NewEvent;
			NewEvent.CoolFactorAmount = CurrentCoolFactor;
			NewEvent.TimeOccurred = CurrentWorldTime;
			CoolFactorHistory.Add(NewEvent);
		}		
	}
}

void AUTPlayerState::SetUniqueId(const TSharedPtr<const FUniqueNetId>& InUniqueId)
{
	Super::SetUniqueId(InUniqueId); 
	ForceUpdatePlayerInfo();
}


void AUTPlayerState::ForceUpdatePlayerInfo()
{
	if (UniqueId.IsValid())
	{
		if (GetWorld())
		{
			AUTGameState* UTGS = GetWorld()->GetGameState<AUTGameState>();
			if (UTGS != nullptr)
			{
				TSharedRef<const FUniqueNetId> UserId = UniqueId.GetUniqueNetId().ToSharedRef();
				if (UserId->IsValid())
				{
					UTGS->AddUserInfoQuery(UserId);
				}
			}
		}
	}
}

void AUTPlayerState::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PC);
	const bool bSpectating = PC && PC->PlayerState && PC->PlayerState->bOnlySpectator;
	const bool bTacCom = bSpectating && UTPC && UTPC->bTacComView;
	const bool bOnSameTeam = GS != NULL && GS->OnSameTeam(PC, this);
	const bool bRecentlyRendered = (GetWorld()->TimeSeconds - GetLastRenderTime() < 0.5f);
	const bool bIsViewTarget = (PC->GetViewTarget() == this);

	if (UTPC != NULL && FVector::DotProduct(CameraDir, (LastPostRenderedLocation - CameraPosition)) > 0.0f && GS != NULL)
	{
		float Dist = (CameraPosition - LastPostRenderedLocation).Size() * FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()*0.5f));
		float TextXL, YL;
		float Scale = Canvas->ClipX / 1920.f;
		UFont* ChatFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->ChatFont;
		Canvas->TextSize(ChatFont, PlayerName, TextXL, YL, Scale, Scale);
		float BarWidth, Y;
		Canvas->TextSize(ChatFont, FString("AAAWWW"), BarWidth, Y, Scale, Scale);
		float MaxBarWidth = 2.f*BarWidth;
		float TextScaling = FMath::Min(1.f, MaxBarWidth / TextXL);
		TextXL *= TextScaling;
		float XL = TextXL + FMath::Max(BarWidth - TextXL, 0.f);
		FVector ScreenPosition = Canvas->Project(LastPostRenderedLocation);
		float XPos = ScreenPosition.X - 0.5f*XL;
		float YPos = ScreenPosition.Y - YL;
		if (XPos < Canvas->ClipX || XPos + XL < 0.0f)
		{
			FLinearColor TeamColor = Team ? Team->TeamColor : FLinearColor::White;
			float Height = 0.75*YL;
			float Border = 2.f*Scale;
			float XDistFromCenter = FMath::Abs(XPos - 0.5f*Canvas->ClipX + 0.5f*XL + Border);
			float YDistFromCenter = FMath::Abs(YPos - 0.5f*Canvas->ClipY + 0.5f*Height + Border);
			float MinBuffer = 0.01f*Canvas->ClipX;
			if ((XDistFromCenter < 0.5*XL + Border + MinBuffer) && (YDistFromCenter < 0.5f*Height + Border + MinBuffer))
			{
				return;
			}
			float PctFromCenter = FVector2D(XDistFromCenter, YDistFromCenter).Size() / Canvas->ClipX;
			float CenterFade = FMath::Clamp(15.f*PctFromCenter - 0.1f, 0.f, 1.f);
			if (CenterFade == 0.f)
			{
				return;
			}
			TeamColor.A = 0.2f * CenterFade;
			UTexture* BarTexture = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->HUDAtlas;
			float SkullHeight = 0.3f*BarWidth;
			Canvas->SetLinearDrawColor(FLinearColor::White);
			Canvas->DrawTile(BarTexture, ScreenPosition.X - 0.5f*SkullHeight, YPos - YL - SkullHeight, SkullHeight, SkullHeight, 725, 0, 28, 36);

			Canvas->SetLinearDrawColor(TeamColor);
			Canvas->DrawTile(Canvas->DefaultTexture, XPos - Border, YPos - YL - Border, XL + 2.f*Border, Height + 2.f*Border, 0, 0, 1, 1);
			FLinearColor BeaconTextColor = FLinearColor::White;
			BeaconTextColor.A = 0.8f * CenterFade;
			FUTCanvasTextItem TextItem(FVector2D(FMath::TruncToFloat(Canvas->OrgX + XPos + 0.5f*(XL - TextXL)), FMath::TruncToFloat(Canvas->OrgY + YPos - 1.2f*YL)), FText::FromString(PlayerName), ChatFont, BeaconTextColor, NULL);
			TextItem.Scale = FVector2D(TextScaling*Scale, TextScaling*Scale);
			TextItem.BlendMode = SE_BLEND_Translucent;
			FLinearColor ShadowColor = FLinearColor::Black;
			ShadowColor.A = BeaconTextColor.A;
			TextItem.EnableShadow(ShadowColor);
			TextItem.FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
			Canvas->DrawItem(TextItem);
		}
	}
}

void AUTPlayerState::ClientTutorialFinished_Implementation(uint16 TutorialMask)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(GetOwner());
	if (PC != NULL)
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
		if (LP != NULL)
		{
			LP->SetTutorialFinished(TutorialMask);
		}
	}
}