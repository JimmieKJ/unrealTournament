// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameMode.h"
#include "UTPlayerState.h"
#include "Engine/LocalPlayer.h"
#include "Net/UnrealNetwork.h"
#include "UTGameMessage.h"
#include "UTHat.h"
#include "UTCharacterContent.h"
#include "../Private/Slate/SUWindowsStyle.h"
#include "Slate/SUWPlayerInfoDialog.h"
#include "Slate/Widgets/SUTTabWidget.h"
#include "StatNames.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "UTReplicatedMapInfo.h"
#include "UTRewardMessage.h"
#include "UTGameEngine.h"
#include "UTFlagInfo.h"
#include "UTEngineMessage.h"
#include "UTGameState.h"

AUTPlayerState::AUTPlayerState(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWaitingPlayer = false;
	bReadyToPlay = false;
	bPendingTeamSwitch = false;
	bCaster = false;
	LastKillTime = 0.0f;
	Kills = 0;
	bOutOfLives = false;
	Deaths = 0;
	bShouldAutoTaunt = false;
	TauntSelectionIndex = 0;

	// We want to be ticked.
	PrimaryActorTick.bCanEverTick = true;

	StatManager = nullptr;
	bReadStatsFromCloud = false;
	bSuccessfullyReadStatsFromCloud = false;
	bWroteStatsToCloud = false;
	DuelSkillRatingThisMatch = 0;
	TDMSkillRatingThisMatch = 0;
	DMSkillRatingThisMatch = 0;
	CTFSkillRatingThisMatch = 0;
	ReadyColor = FLinearColor::White;
	ReadyScale = 1.f;
	SpectatorNameScale = 1.f;
	bIsDemoRecording = false;
	EngineMessageClass = UUTEngineMessage::StaticClass();
	LastTauntTime = -1000.f;

	static ConstructorHelpers::FObjectFinder<UClass> DefaultVoice(TEXT("BlueprintGeneratedClass'/Game/RestrictedAssets/Character/DefaultMaleVoice.DefaultMaleVoice_C'"));
	CharacterVoice = DefaultVoice.Object;
}

void AUTPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTPlayerState, CarriedObject);
	DOREPLIFETIME(AUTPlayerState, bWaitingPlayer);
	DOREPLIFETIME(AUTPlayerState, bReadyToPlay);
	DOREPLIFETIME(AUTPlayerState, bPendingTeamSwitch);
	DOREPLIFETIME(AUTPlayerState, bOutOfLives);
	DOREPLIFETIME(AUTPlayerState, Kills);
	DOREPLIFETIME(AUTPlayerState, Deaths);
	DOREPLIFETIME(AUTPlayerState, Team);
	DOREPLIFETIME(AUTPlayerState, FlagCaptures);
	DOREPLIFETIME(AUTPlayerState, FlagReturns);
	DOREPLIFETIME(AUTPlayerState, Assists);
	DOREPLIFETIME(AUTPlayerState, LastKillerPlayerState);
	DOREPLIFETIME(AUTPlayerState, bHasHighScore);
	DOREPLIFETIME(AUTPlayerState, ChatDestination);
	DOREPLIFETIME(AUTPlayerState, CountryFlag);
	DOREPLIFETIME(AUTPlayerState, AverageRank);
	DOREPLIFETIME(AUTPlayerState, TrainingLevel);
	DOREPLIFETIME(AUTPlayerState, SelectedCharacter);
	DOREPLIFETIME(AUTPlayerState, TauntClass);
	DOREPLIFETIME(AUTPlayerState, Taunt2Class);
	DOREPLIFETIME(AUTPlayerState, HatClass);
	DOREPLIFETIME(AUTPlayerState, EyewearClass);
	DOREPLIFETIME(AUTPlayerState, HatVariant);
	DOREPLIFETIME(AUTPlayerState, EyewearVariant);
	DOREPLIFETIME(AUTPlayerState, bSpecialPlayer);
	DOREPLIFETIME(AUTPlayerState, OverrideHatClass);
	DOREPLIFETIME(AUTPlayerState, Loadout);
	DOREPLIFETIME(AUTPlayerState, KickPercent);
	DOREPLIFETIME(AUTPlayerState, AvailableCurrency);
	DOREPLIFETIME(AUTPlayerState, StatsID);

	DOREPLIFETIME_CONDITION(AUTPlayerState, RespawnChoiceA, COND_None); // also used when replicating spawn choice to other players
	DOREPLIFETIME_CONDITION(AUTPlayerState, RespawnChoiceB, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, bChosePrimaryRespawnChoice, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, WeaponSpreeDamage, COND_OwnerOnly);

	DOREPLIFETIME(AUTPlayerState, SpectatingID);
	DOREPLIFETIME(AUTPlayerState, SpectatingIDTeam);
	DOREPLIFETIME(AUTPlayerState, bCaster);
	DOREPLIFETIME_CONDITION(AUTPlayerState, bIsDemoRecording, COND_InitialOnly);
}

void AUTPlayerState::Destroyed()
{
	Super::Destroyed();
	GetWorldTimerManager().ClearAllTimersForObject(this);
}

void AUTPlayerState::SetPlayerName(const FString& S)
{
	Super::SetPlayerName(S);
	bHasValidClampedName = false;
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
	}
}

bool AUTPlayerState::ShouldAutoTaunt() const
{
	return bIsABot || bShouldAutoTaunt;
}

void AUTPlayerState::AnnounceKill()
{
	if (CharacterVoice && ShouldAutoTaunt() && GetWorld()->GetAuthGameMode())
	{
		int32 NumTaunts = CharacterVoice.GetDefaultObject()->TauntMessages.Num();
		if (NumTaunts > 0)
		{
			int32 SelectedTaunt = TauntSelectionIndex + FMath::Min(FMath::RandRange(0, 2), NumTaunts - TauntSelectionIndex - 1);
			TauntSelectionIndex += 3;
			if (TauntSelectionIndex >= NumTaunts)
			{
				TauntSelectionIndex -= FMath::Max(3, NumTaunts);
			}
			GetWorld()->GetAuthGameMode()->BroadcastLocalized(GetOwner(), CharacterVoice, SelectedTaunt, this);
		}
	}
}

void AUTPlayerState::AnnounceSameTeam(AUTPlayerController* ShooterPC)
{
	if (CharacterVoice && ShouldAutoTaunt() && GetWorld()->GetAuthGameMode() && (GetWorld()->GetTimeSeconds() - ShooterPC->LastSameTeamTime > 5.f) 
		&& (CharacterVoice.GetDefaultObject()->SameTeamMessages.Num() > 0))
	{
		ShooterPC->LastSameTeamTime = GetWorld()->GetTimeSeconds();
		ShooterPC->ClientReceiveLocalizedMessage(CharacterVoice, CharacterVoice.GetDefaultObject()->SameTeamBaseIndex + FMath::RandRange(0, CharacterVoice.GetDefaultObject()->SameTeamMessages.Num() - 1), this, ShooterPC->PlayerState, NULL);
	}
}

void AUTPlayerState::AnnounceReactionTo(const AUTPlayerState* ReactionPS) const
{
	if (CharacterVoice && ShouldAutoTaunt() && GetWorld()->GetAuthGameMode())
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
			else if (bSameTeam || (NumEnemyReactions == 0))
			{
				return;
			}
		}
		PC->ClientReceiveLocalizedMessage(CharacterVoice, CharacterVoice.GetDefaultObject()->EnemyReactionBaseIndex + FMath::RandRange(0, CharacterVoice.GetDefaultObject()->EnemyReactions.Num() - 1), const_cast<AUTPlayerState*>(this), PC->PlayerState, NULL);
	}
}

void AUTPlayerState::AnnounceStatus(FName NewStatus)
{
	if (CharacterVoice && ShouldAutoTaunt())
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
			// no valid index found (NewStatus was not a valid selection)
			return;
		}
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
			if (PC && GS->OnSameTeam(this, PC))
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
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AUTCharacter* P = Cast<AUTCharacter>(*It);
		if (P != NULL && P->PlayerState == this && !P->bTearOff)
		{
			P->NotifyTeamChanged();
		}
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
			}
		}
	}
}

void AUTPlayerState::SetWaitingPlayer(bool B)
{
	bIsSpectator = B;
	bWaitingPlayer = B;
	ForceNetUpdate();
}

void AUTPlayerState::IncrementKills(TSubclassOf<UDamageType> DamageType, bool bEnemyKill, AUTPlayerState* VictimPS)
{
	if (bEnemyKill)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
		AController* Controller = Cast<AController>(GetOwner());
		APawn* Pawn = nullptr;
		AUTCharacter* UTChar = nullptr;
		bool bShouldTauntKill = false;
		if (Controller)
		{
			Pawn = Controller->GetPawn();
			UTChar = Cast<AUTCharacter>(Pawn);
		}
		AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetOwner());
		TSubclassOf<UUTDamageType> UTDamage(*DamageType);

		if (GS != NULL && GetWorld()->TimeSeconds - LastKillTime < GS->MultiKillDelay)
		{
			FName MKStat[4] = { NAME_MultiKillLevel0, NAME_MultiKillLevel1, NAME_MultiKillLevel2, NAME_MultiKillLevel3 };
			ModifyStatsValue(MKStat[FMath::Min(MultiKillLevel, 3)], 1);
			MultiKillLevel++;
			bShouldTauntKill = true;
			if (MyPC != NULL)
			{
				MyPC->SendPersonalMessage(GS->MultiKillMessageClass, MultiKillLevel - 1, this, VictimPS);
			}

			if (GM)
			{
				GM->AddMultiKillEventToReplay(Controller);
			}
		}
		else
		{
			MultiKillLevel = 0;
		}

		bool bAnnounceWeaponSpree = false;
		if (UTDamage)
		{
			if (!UTDamage.GetDefaultObject()->StatsName.IsEmpty())
			{
				// FIXMESTEVE - preset, not constructed FName
				ModifyStatsValue(FName(*(UTDamage.GetDefaultObject()->StatsName + TEXT("Kills"))), 1);
			}
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
				ModifyStatsValue(SKStat[FMath::Min(Spree / 5, 4)], 1);
				bShouldTauntKill = true;

				if (GetWorld()->GetAuthGameMode() != NULL)
				{
					GetWorld()->GetAuthGameMode()->BroadcastLocalized(GetOwner(), GS->SpreeMessageClass, Spree / 5, this, VictimPS);
				}

				if (UTChar != NULL && UTChar->IsWearingAnyCosmetic())
				{
					UTChar->CosmeticSpreeCount = FMath::Min(Spree / 5, 4);
					UTChar->OnRepCosmeticSpreeCount();
				}

				if (GM)
				{
					GM->AddSpreeKillEventToReplay(Controller, FMath::Min(Spree / 5, 4));
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

		if (bAnnounceWeaponSpree)
		{
			AnnounceWeaponSpree(UTDamage);
		}
		bool bVictimIsLocalPlayer = (Cast<AUTPlayerController>(VictimPS->GetOwner()) != NULL);
		bShouldTauntKill = bShouldTauntKill || (GM && (GetWorld()->GetTimeSeconds() - GM->LastGlobalTauntTime > 15.f)) || bVictimIsLocalPlayer;
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
	if (Spree >= 5 && GetWorld()->GetAuthGameMode() != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			GetWorld()->GetAuthGameMode()->BroadcastLocalized(GetOwner(), GS->SpreeMessageClass, Spree / -5, this, KillerPlayerState);
		}
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

void AUTPlayerState::OnDeathsReceived()
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState != NULL)
	{
		RespawnTime = UTGameState->RespawnWaitTime;
		ForceRespawnTime = UTGameState->ForceRespawnTime;
	}
}

void AUTPlayerState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// If we are waiting to respawn then count down
	if (RespawnTime > 0.0f)
	{
		RespawnTime -= DeltaTime;
	}

	if (ForceRespawnTime > 0.0f)
	{
		ForceRespawnTime -= DeltaTime;
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
	if (CachedCharacter && CachedCharacter->PlayerState == this)
	{
		return CachedCharacter;
	}

	// iterate through all pawns and find matching playerstate ref
	for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
	{
		AUTCharacter* UTChar = Cast<AUTCharacter>(*Iterator);
		if (UTChar && UTChar->PlayerState == this)
		{
			CachedCharacter = UTChar;
			return UTChar;
		}
	}

	return nullptr;
}

void AUTPlayerState::OnRepHat()
{
	AUTCharacter* UTChar = GetUTCharacter();
	if (UTChar != nullptr)
	{
		UTChar->SetHatClass(HatClass);
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
	HatClass = LoadClass<AUTHat>(NULL, *NewHatClass, NULL, LOAD_NoWarn, NULL);

	// Allow the game mode to validate the hat.
	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if ( GameMode && !GameMode->ValidateHat(this, NewHatClass) )
	{
		return;
	}

	if (HatClass != nullptr && !HatClass->IsChildOf(AUTHatLeader::StaticClass()))
	{
		OnRepHat();
	}
	else
	{
		HatClass = nullptr;
	}

	if (HatClass != nullptr)
	{
		ValidateEntitlements();
	}
}

bool AUTPlayerState::ServerReceiveHatClass_Validate(const FString& NewHatClass)
{
	return true;
}

void AUTPlayerState::ServerReceiveEyewearClass_Implementation(const FString& NewEyewearClass)
{
	EyewearClass = LoadClass<AUTEyewear>(NULL, *NewEyewearClass, NULL, LOAD_NoWarn, NULL);
	OnRepEyewear();
	if (EyewearClass != NULL)
	{
		ValidateEntitlements();
	}
}

bool AUTPlayerState::ServerReceiveEyewearClass_Validate(const FString& NewEyewearClass)
{
	return true;
}

void AUTPlayerState::ServerReceiveTauntClass_Implementation(const FString& NewTauntClass)
{
	TauntClass = LoadClass<AUTTaunt>(NULL, *NewTauntClass, NULL, LOAD_NoWarn, NULL);
	if (TauntClass != NULL)
	{
		ValidateEntitlements();
	}
}

bool AUTPlayerState::ServerReceiveTauntClass_Validate(const FString& NewEyewearClass)
{
	return true;
}

void AUTPlayerState::ServerReceiveTaunt2Class_Implementation(const FString& NewTauntClass)
{
	Taunt2Class = LoadClass<AUTTaunt>(NULL, *NewTauntClass, NULL, LOAD_NoWarn, NULL);
	if (Taunt2Class != NULL)
	{
		ValidateEntitlements();
	}
}

bool AUTPlayerState::ServerReceiveTaunt2Class_Validate(const FString& NewEyewearClass)
{
	return true;
}

void AUTPlayerState::HandleTeamChanged(AController* Controller)
{
	AUTCharacter* Pawn = Cast<AUTCharacter>(Controller->GetPawn());
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
		PS->Team = Team;
		PS->bReadStatsFromCloud = bReadStatsFromCloud;
		PS->bSuccessfullyReadStatsFromCloud = bSuccessfullyReadStatsFromCloud;
		PS->bWroteStatsToCloud = bWroteStatsToCloud;
		PS->DuelSkillRatingThisMatch = DuelSkillRatingThisMatch;
		PS->DMSkillRatingThisMatch = DMSkillRatingThisMatch;
		PS->TDMSkillRatingThisMatch = TDMSkillRatingThisMatch;
		PS->CTFSkillRatingThisMatch = CTFSkillRatingThisMatch;
		PS->StatsID = StatsID;
		PS->Kills = Kills;
		PS->Deaths = Deaths;
		PS->Assists = Assists;
		PS->HatClass = HatClass;
		PS->EyewearClass = EyewearClass;
		PS->HatVariant = HatVariant;
		PS->EyewearVariant = EyewearVariant;
		PS->SelectedCharacter = SelectedCharacter;
		PS->StatManager = StatManager;
		PS->StatsData = StatsData;
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

void AUTPlayerState::SetCharacter(const FString& CharacterPath)
{
	if (Role == ROLE_Authority)
	{
		SelectedCharacter = (CharacterPath.Len() > 0) ? LoadClass<AUTCharacterContent>(NULL, *CharacterPath, NULL, LOAD_None, NULL) : NULL;
// redirect from blueprint, for easier testing in the editor via C/P
#if WITH_EDITORONLY_DATA
		if (SelectedCharacter == NULL)
		{
			UBlueprint* BP = LoadObject<UBlueprint>(NULL, *CharacterPath, NULL, LOAD_None, NULL);
			if (BP != NULL)
			{
				SelectedCharacter = *BP->GeneratedClass;
			}
		}
#endif
		NotifyTeamChanged();
	}
}

bool AUTPlayerState::ServerSetCharacter_Validate(const FString& CharacterPath)
{
	return true;
}
void AUTPlayerState::ServerSetCharacter_Implementation(const FString& CharacterPath)
{
	// TODO: maybe ignore if already have valid character to avoid trolls?
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

bool AUTPlayerState::ModifyStat(FName StatName, int32 Amount, EStatMod::Type ModType)
{
	if (StatManager != nullptr)
	{
		return StatManager->ModifyStat(StatName, Amount, ModType);
	}

	return false;
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

void AUTPlayerState::SetUniqueId(const TSharedPtr<FUniqueNetId>& InUniqueId)
{
	Super::SetUniqueId(InUniqueId);

	if (Role == ROLE_Authority && InUniqueId.IsValid())
	{
		ReadProfileItems();
	}
}

void AUTPlayerState::ReadProfileItems()
{
	if (!IsProfileItemListPending() && UniqueId.IsValid())
	{
		FHttpRequestCompleteDelegate Delegate;
		Delegate.BindUObject(this, &AUTPlayerState::ProfileItemListReqComplete);
		ItemListReq = ReadBackendStats(Delegate, UniqueId->ToString());
	}
}

void AUTPlayerState::ProfileItemListReqComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (bSucceeded)
	{
		ParseProfileItemJson(HttpResponse->GetContentAsString(), ProfileItems);
	}
	ItemListReq.Reset();
	ValidateEntitlements();
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

					DuelSkillRatingThisMatch = StatManager->GetStatValueByName(FName((TEXT("SkillRating"))));
					TDMSkillRatingThisMatch = StatManager->GetStatValueByName(FName((TEXT("TDMSkillRating"))));
					DMSkillRatingThisMatch = StatManager->GetStatValueByName(FName((TEXT("DMSkillRating"))));
					CTFSkillRatingThisMatch = StatManager->GetStatValueByName(FName((TEXT("CTFSkillRating"))));
				}
			}
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

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			FString BaseURL = TEXT("https://ut-public-service-gamedev.ol.epicgames.net/ut/api/stats/accountId/") + StatsID + TEXT("/bulk?ownertype=1");
#else
			FString BaseURL = TEXT("https://ut-public-service-prod10.ol.epicgames.com/ut/api/stats/accountId/") + StatsID + TEXT("/bulk?ownertype=1");
#endif

			FString McpConfigOverride;
			FParse::Value(FCommandLine::Get(), TEXT("MCPCONFIG="), McpConfigOverride);

			if (McpConfigOverride == TEXT("prodnet"))
			{
				BaseURL = TEXT("https://ut-public-service-prod10.ol.epicgames.com/ut/api/stats/accountId/") + StatsID + TEXT("/bulk?ownertype=1");
			}
			else if (McpConfigOverride == TEXT("localhost"))
			{
				BaseURL = TEXT("http://localhost:8080/ut/api/stats/accountId/") + StatsID + TEXT("/bulk?ownertype=1");
			}
			else if (McpConfigOverride == TEXT("gamedev"))
			{
				BaseURL = TEXT("https://ut-public-service-gamedev.ol.epicgames.net/ut/api/stats/accountId/") + StatsID + TEXT("/bulk?ownertype=1");
			}

			FHttpRequestPtr StatsWriteRequest = FHttpModule::Get().CreateRequest();
			if (StatsWriteRequest.IsValid())
			{
				StatsWriteRequest->SetURL(BaseURL);
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

void AUTPlayerState::AddMatchToStats(const FString& GameType, const TArray<class AUTTeamInfo*>* Teams, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates)
{
	if (StatManager != nullptr && !StatsID.IsEmpty())
	{
		StatManager->AddMatchToStats(GameType, Teams, ActivePlayerStates, InactivePlayerStates);
	}
}

// Don't access the skill rating in the statmanager so UpdateSkillRating() can rely on this function
int32 AUTPlayerState::GetSkillRating(FName SkillStatName)
{
	int32 SkillRating = 0;
		
	if (SkillStatName == FName(TEXT("SkillRating")))
	{
		SkillRating = DuelSkillRatingThisMatch;
	}
	else if (SkillStatName == FName(TEXT("TDMSkillRating")))
	{
		SkillRating = TDMSkillRatingThisMatch;
	}
	else if (SkillStatName == FName(TEXT("DMSkillRating")))
	{
		SkillRating = DMSkillRatingThisMatch;
	}
	else if (SkillStatName == FName(TEXT("CTFSkillRating")))
	{
		SkillRating = CTFSkillRatingThisMatch;
	}
	else
	{
		UE_LOG(LogGameStats, Warning, TEXT("GetSkillRating could not find skill rating for %s"), *SkillStatName.ToString());
	}

	// SkillRating was unset previously or a broken value, return the starting value
	if (SkillRating <= 0)
	{
		SkillRating = 1500;
	}

	// Going to try bots out at 1200, meaning you'll get ~4 ELO per win at 1500 in a bot grind match and ~3 at 1600
	// 70 bot matches to climb to silver seems fair for now
	// Once a player get near 1950 ELO (assuming KFactor 32), a win against a 1200 will be 0 points making it impossible to grind bot wins to gold
	if (bIsABot)
	{
		SkillRating = 1200;
	}

	return SkillRating;
}

void AUTPlayerState::UpdateTeamSkillRating(FName SkillStatName, bool bWonMatch, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates)
{
	// Not writing stats for this player
	if (StatManager == nullptr || StatsID.IsEmpty())
	{
		return;
	}

	int32 SkillRating = GetSkillRating(SkillStatName);
	
	int32 OpponentCount = 0;
	float ExpectedWinPercentage = 0.0f;
	for (int32 OuterPlayerIdx = 0; OuterPlayerIdx < ActivePlayerStates->Num(); OuterPlayerIdx++)
	{
		AUTPlayerState* Opponent = Cast<AUTPlayerState>((*ActivePlayerStates)[OuterPlayerIdx]);
		if (Opponent->Team != Team && !Opponent->bOnlySpectator)
		{
			OpponentCount++;
			int32 OpponentSkillRating = Opponent->GetSkillRating(SkillStatName);
			ExpectedWinPercentage += 1.0f / (1.0f + pow(10.0f, (float(OpponentSkillRating - SkillRating) / 400.0f)));
		}
	}
	for (int32 OuterPlayerIdx = 0; OuterPlayerIdx < InactivePlayerStates->Num(); OuterPlayerIdx++)
	{
		AUTPlayerState* Opponent = Cast<AUTPlayerState>((*InactivePlayerStates)[OuterPlayerIdx]);
		if (Opponent && Opponent->Team != Team && !Opponent->bOnlySpectator)
		{
			OpponentCount++;
			int32 OpponentSkillRating = Opponent->GetSkillRating(SkillStatName);
			ExpectedWinPercentage += 1.0f / (1.0f + pow(10.0f, (float(OpponentSkillRating - SkillRating) / 400.0f)));
		}
	}

	UE_LOG(LogGameStats, Log, TEXT("UpdateTeamSkillRating %s RA:%d E:%f"), *PlayerName, SkillRating, ExpectedWinPercentage);

	if (OpponentCount == 0)
	{
		UE_LOG(LogGameStats, Log, TEXT("UpdateTeamSkillRating %s no opponents found, can't adjust skill rating"), *PlayerName);
		return;
	}

	// KFactor selection can be chosen many different ways, feel free to change it
	float KFactor = 32.0f / float(OpponentCount);
	if (SkillRating > 2400)
	{
		KFactor = 16.0f / float(OpponentCount);
	}
	else if (SkillRating >= 2100)
	{
		KFactor = 24.0f / float(OpponentCount);
	}

	int32 NewSkillRating = 0;
	if (bWonMatch)
	{
		NewSkillRating = SkillRating + KFactor*(float(OpponentCount) - ExpectedWinPercentage);
	}
	else
	{
		NewSkillRating = SkillRating + KFactor*(0.0f - ExpectedWinPercentage);
	}

	UE_LOG(LogGameStats, Log, TEXT("UpdateTeamSkillRating %s New Skill Rating %d"), *PlayerName, NewSkillRating);
	ModifyStat(SkillStatName, NewSkillRating, EStatMod::Set);
	ModifyStat(FName(*(SkillStatName.ToString() + TEXT("Samples"))), 1, EStatMod::Delta);
}

void AUTPlayerState::UpdateIndividualSkillRating(FName SkillStatName, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates)
{
	// Not writing stats for this player
	if (StatManager == nullptr || StatsID.IsEmpty())
	{
		return;
	}

	int32 SkillRating = GetSkillRating(SkillStatName);

	int32 OpponentCount = 0;
	float ExpectedWinPercentage = 0.0f;
	float ActualWinPercentage = 0.0f;
	for (int32 OuterPlayerIdx = 0; OuterPlayerIdx < ActivePlayerStates->Num(); OuterPlayerIdx++)
	{
		AUTPlayerState* Opponent = Cast<AUTPlayerState>((*ActivePlayerStates)[OuterPlayerIdx]);
		if (Opponent != this && !Opponent->bOnlySpectator)
		{
			OpponentCount++;
			int32 OpponentSkillRating = Opponent->GetSkillRating(SkillStatName);
			ExpectedWinPercentage += 1.0f / (1.0f + pow(10.0f, (float(OpponentSkillRating - SkillRating) / 400.0f)));

			if (Score > Opponent->Score)
			{
				ActualWinPercentage += 1.0f;
			}
			else if (Score == Opponent->Score)
			{
				ActualWinPercentage += 0.5f;
			}
		}
	}
	for (int32 OuterPlayerIdx = 0; OuterPlayerIdx < InactivePlayerStates->Num(); OuterPlayerIdx++)
	{
		AUTPlayerState* Opponent = Cast<AUTPlayerState>((*InactivePlayerStates)[OuterPlayerIdx]);
		if (Opponent && Opponent != this && !Opponent->bOnlySpectator)
		{
			OpponentCount++;
			int32 OpponentSkillRating = Opponent->GetSkillRating(SkillStatName);
			ExpectedWinPercentage += 1.0f / (1.0f + pow(10.0f, (float(OpponentSkillRating - SkillRating) / 400.0f)));

			if (Score > Opponent->Score)
			{
				ActualWinPercentage += 1.0f;
			}
			else if (Score == Opponent->Score)
			{
				ActualWinPercentage += 0.5f;
			}
		}
	}

	UE_LOG(LogGameStats, Log, TEXT("UpdateIndividualSkillRating %s RA:%d E:%f"), *PlayerName, SkillRating, ExpectedWinPercentage);

	if (OpponentCount == 0)
	{
		UE_LOG(LogGameStats, Log, TEXT("UpdateIndividualSkillRating %s no opponents found, can't adjust skill rating"), *PlayerName);
		return;
	}

	// KFactor selection can be chosen many different ways, feel free to change it
	float KFactor = 32.0f / float(OpponentCount);
	if (SkillRating > 2400)
	{
		KFactor = 16.0f / float(OpponentCount);
	}
	else if (SkillRating >= 2100)
	{
		KFactor = 24.0f / float(OpponentCount);
	}

	int32 NewSkillRating = SkillRating + KFactor*(ActualWinPercentage - ExpectedWinPercentage);

	UE_LOG(LogGameStats, Log, TEXT("UpdateIndividualSkillRating %s New Skill Rating %d"), *PlayerName, NewSkillRating);
	ModifyStat(SkillStatName, NewSkillRating, EStatMod::Set);
	ModifyStat(FName(*(SkillStatName.ToString() + TEXT("Samples"))), 1, EStatMod::Delta);
}

bool AUTPlayerState::OwnsItemFor(const FString& Path, int32 VariantId) const
{
	for (const FProfileItemEntry& Entry : ProfileItems)
	{
		if (Entry.Item != NULL && Entry.Item->Grants(Path, VariantId))
		{
			return true;
		}
	}
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
				if (!HasRightsFor(EyewearClass))
				{
					ServerReceiveEyewearClass(FString());
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

/**
 *	Find the current first local player and asks if I'm their friend.  NOTE: the UTLocalPlayer will, when he friends list changes
 *  update these as well.
 **/
void AUTPlayerState::OnRep_UniqueId()
{
	Super::OnRep_UniqueId();
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(GetWorld(),0));
	if (LP)
	{
		bIsFriend = LP->IsAFriend(UniqueId);
	}
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

const FSlateBrush* AUTPlayerState::GetELOBadgeImage() const
{
	int32 Badge = 0;
	int32 Level = 0;

	UUTLocalPlayer::GetBadgeFromELO(AverageRank, Badge, Level);
	FString BadgeStr = FString::Printf(TEXT("UT.Badge.%i"), Badge);
	return SUWindowsStyle::Get().GetBrush(*BadgeStr);
}

const FSlateBrush* AUTPlayerState::GetELOBadgeNumberImage() const
{
	int32 Badge = 0;
	int32 Level = 0;

	UUTLocalPlayer::GetBadgeFromELO(AverageRank, Badge, Level);
	FString BadgeNumberStr = FString::Printf(TEXT("UT.Badge.Numbers.%i"), FMath::Clamp<int32>(Level + 1, 1, 9));
	return SUWindowsStyle::Get().GetBrush(*BadgeNumberStr);
}

void AUTPlayerState::BuildPlayerInfo(TSharedPtr<SUTTabWidget> TabWidget, TArray<TSharedPtr<TAttributeStat> >& StatList)
{
	UUTFlagInfo* Flag = Cast<UUTGameEngine>(GEngine) ? Cast<UUTGameEngine>(GEngine)->GetFlag(CountryFlag) : nullptr;

	TabWidget->AddTab(NSLOCTEXT("AUTPlayerState", "PlayerInfo", "Player Info"), 
	SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.Padding(10.0f, 5.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(150)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Generic", "PlayerNamePrompt", "Name :"))
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.ColorAndOpacity(FLinearColor::Gray)
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(5.0,0.0,0.0,0.0)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(PlayerName))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
			]
		]
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.WidthOverride(32)
				.HeightOverride(32)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(GetELOBadgeImage())
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					[
						SNew(SImage)
						.Image(GetELOBadgeNumberImage())
					]
				]
			]
		]
	]
	+ SVerticalBox::Slot()
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
			.WidthOverride(150)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Generic", "PlayerFlagPrompt", ""))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
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
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Flag ? Flag->GetFriendlyName() : TEXT("")))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
		]
	]
	+SVerticalBox::Slot()
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
			.WidthOverride(150)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Generic", "ScorePrompt", "Score :"))
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
			SNew(STextBlock)
			.Text(FText::AsNumber(Score))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
		]
	]

	+SVerticalBox::Slot()
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
			.WidthOverride(150)
			[
				SNew(STextBlock)
				.Text(bIsABot ? NSLOCTEXT("Generic", "SkillPrompt", "Skill Level ") : NSLOCTEXT("Generic", "RankPrompt", "Rank :"))
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
			SNew(STextBlock)
			.Text(FText::AsNumber(AverageRank))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
		]
	]
	+SVerticalBox::Slot()
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
			.WidthOverride(150)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Generic", "EpicIDPrompt", "ID :"))
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
			SNew(SHyperlink)
			.Text(FText::FromString(StatsID))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
			.OnNavigate(FSimpleDelegate::CreateUObject(this, &AUTPlayerState::EpicIDClicked))
		]
	]
	+SVerticalBox::Slot()
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
			.WidthOverride(150)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Generic", "TrainingPrompt", "Training :"))
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
			SNew(STextBlock)
			.Text(GetTrainingLevelText())
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
		]
	]);
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

void AUTPlayerState::OnRep_PlayerName()
{
	Super::OnRep_PlayerName();
	bHasValidClampedName = false;
}


void AUTPlayerState::SetOverrideHatClass(const FString& NewOverrideHatClass)
{
	OverrideHatClass = NewOverrideHatClass == TEXT("") ? nullptr : LoadClass<AUTHat>(NULL, *NewOverrideHatClass, NULL, LOAD_NoWarn, NULL);
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

void AUTPlayerState::UpdateReady()
{
	uint8 NewReadyState = bReadyToPlay + (bPendingTeamSwitch >> 2);
	if ((ReadySwitchCount > 2) && bReadyToPlay)
	{
		if (GetWorld()->GetTimeSeconds() - LastReadySwitchTime > 0.2f)
		{
			ReadySwitchCount++;
			LastReadySwitchTime = GetWorld()->GetTimeSeconds();
			if ((ReadySwitchCount & 14) == 0)
			{
				ReadySwitchCount += 2;
			}
			ReadyColor.R = (ReadySwitchCount & 2) ? 1.f : 0.f;
			ReadyColor.G = (ReadySwitchCount & 4) ? 1.f : 0.f;
			ReadyColor.B = (ReadySwitchCount & 8) ? 1.f : 0.f;
		}
		if (ReadySwitchCount > 10)
		{
			ReadyScale = 1.f + 2.f*(GetWorld()->GetTimeSeconds() - LastReadySwitchTime);
		}
	}
	else if (!bReadyToPlay && (GetWorld()->GetTimeSeconds() - LastReadySwitchTime > 0.5f))
	{
		ReadySwitchCount = 0;
		ReadyColor = FLinearColor::White;
		ReadyScale = 1.f;
	}
	else if (NewReadyState != LastReadyState)
	{
		ReadySwitchCount = (GetWorld()->GetTimeSeconds() - LastReadySwitchTime < 0.5f) ? ReadySwitchCount + 1 : 0;
		LastReadySwitchTime = GetWorld()->GetTimeSeconds();
		ReadyScale = 1.f;
	}
	LastReadyState = NewReadyState;
}

void AUTPlayerState::LogBanRequest(AUTPlayerState* Voter)
{
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
					if (GameState->PlayerArray[i]->UniqueId == BanVotes[Idx].Voter)
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
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AUTCharacter* Character = Cast<AUTCharacter>(*It);
		if (Character != NULL && Character->PlayerState == this && !Character->bTearOff)
		{
			Character->UpdateTacComMesh(bSpecialPlayer);
		}
	}
}

float AUTPlayerState::GetStatsValue(FName StatsName) const
{
	return StatsData.FindRef(StatsName);
}

void AUTPlayerState::SetStatsValue(FName StatsName, float NewValue)
{
	LastScoreStatsUpdateTime = GetWorld()->GetTimeSeconds();
	StatsData.Add(StatsName, NewValue);
}

void AUTPlayerState::ModifyStatsValue(FName StatsName, float Change)
{
	LastScoreStatsUpdateTime = GetWorld()->GetTimeSeconds();
	float CurrentValue = StatsData.FindRef(StatsName);
	StatsData.Add(StatsName, CurrentValue + Change);
}

bool AUTPlayerState::RegisterVote_Validate(AUTReplicatedMapInfo* VoteInfo) { return true; }
void AUTPlayerState::RegisterVote_Implementation(AUTReplicatedMapInfo* VoteInfo)
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState)
	{
		for (int32 i=0; i< UTGameState->MapVoteList.Num(); i++)
		{
			if (UTGameState->MapVoteList[i]) UTGameState->MapVoteList[i]->UnregisterVoter(this);
		}

		VoteInfo->RegisterVoter(this);
	}
}

void AUTPlayerState::OnRep_bIsInactive()
{
	//Now that we replicate InactivePRI's the super function is unsafe without these checks
	if (GetWorld() && GetWorld()->GameState)
	{
		Super::OnRep_bIsInactive();
	}
}