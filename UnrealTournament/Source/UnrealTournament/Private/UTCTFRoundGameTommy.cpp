// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeamGameMode.h"
#include "UTHUD_CTF.h"
#include "UTCTFRoundGameTommy.h"
#include "UTCTFGameMessage.h"
#include "UTCTFRoleMessage.h"
#include "UTCTFRewardMessage.h"
#include "UTFirstBloodMessage.h"
#include "UTCountDownMessage.h"
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
#include "UTSkullPickup.h"
#include "UTArmor.h"
#include "UTTimedPowerup.h"

AUTCTFRoundGameTommy::AUTCTFRoundGameTommy(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DisplayName = NSLOCTEXT("UTGameMode", "RCTFT", "TommyTestMode");

	ChestArmorObject = FStringAssetReference(TEXT("/Game/RestrictedAssets/Pickups/Armor/Armor_Chest.Armor_Chest_C"));

	TimeLimit = 3;

	OffenseKills = 0;
	OffenseKillsNeededForPowerUp = 10;

	DefenseKills = 0;
	DefenseKillsNeededForPowerUp = 10;
}

void AUTCTFRoundGameTommy::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	
	if (!ChestArmorObject.IsNull())
	{
		ChestArmorClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *ChestArmorObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}
}

void AUTCTFRoundGameTommy::InitRound()
{
	Super::InitRound();

	OffenseKills = 0;

	//Fixup for Differences between RCTFT and RCTF
	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		PS->RemainingBoosts = 0; //No boosts by default
		PS->RespawnWaitTime = UnlimitedRespawnWaitTime; //No respawn time limits by default
	}
}

void AUTCTFRoundGameTommy::GiveDefaultInventory(APawn* PlayerPawn)
{
	//Not calling Super::GiveDefaultInventory because we want different default inventory loadouts over the base RCTF mode, so skipping AUTCTFRoundGame::GiveDefaultInventory
	AUTCTFBaseGame::GiveDefaultInventory(PlayerPawn);
	
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(PlayerPawn);
	if (UTCharacter != NULL)
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(UTCharacter->PlayerState);
		//Give defenders extra stuff	
		if (IsTeamOnDefense(UTPlayerState->Team->TeamIndex))
		{
			UTCharacter->AddInventory(GetWorld()->SpawnActor<AUTInventory>(ThighPadClass, FVector(0.0f), FRotator(0.f, 0.f, 0.f)), true);
			UTCharacter->AddInventory(GetWorld()->SpawnActor<AUTInventory>(ChestArmorClass, FVector(0.0f), FRotator(0.f, 0.f, 0.f)), true);
		}
	}
}

void AUTCTFRoundGameTommy::ScoreOutOfLives(int32 WinningTeamIndex)
{
	//we want to give +2 points on an offense win so that they don't have to run the flag with no defender. 
	//The Super call will already give +1, so lets add a pre-emptive +1 before the call
	if (IsTeamOnOffense(WinningTeamIndex))
	{
		AUTTeamInfo* WinningTeam = (Teams.Num() > WinningTeamIndex) ? Teams[WinningTeamIndex] : NULL;
		if (WinningTeam)
		{
			WinningTeam->Score++;
		}
	}

	Super::ScoreOutOfLives(WinningTeamIndex);
}

void AUTCTFRoundGameTommy::ToggleSpecialFor(AUTCharacter* C)
{
	AUTPlayerState* PS = C ? Cast<AUTPlayerState>(C->PlayerState) : nullptr;
	if (PS && (PS->RemainingBoosts > 0))
	{	
		//offense has same Special as RCTF base game
		if (IsTeamOnOffense(PS->Team->TeamIndex))
		{
			Super::ToggleSpecialFor(C);
		}
		else
		{
			PS->RemainingBoosts--;
			//FIXMETOMMY - This is a ghetto way to implemnt this. Lets not suicide in the future, but fine for testing the idea.
			AUTPlayerController* PC = Cast<AUTPlayerController>(PS->GetOwner());
			if (PC)
			{
				//Defense gets a respawn without it costing them a life
				PS->RemainingLives++;

				//Respawn
				C->PlayerSuicide();
			}
		}
	}
}

void AUTCTFRoundGameTommy::DiscardInventory(APawn* Other, AController* Killer)
{
	AUTPlayerState* KillerPS = Killer ? Cast<AUTPlayerState>(Killer->PlayerState) : nullptr;
	AUTPlayerState* VictimPS = Other ? Cast<AUTPlayerState>(Other->PlayerState) : nullptr;

	if (VictimPS && VictimPS->Team && KillerPS && KillerPS->Team)
	{
		//No credit for suicides
		if (VictimPS->Team->TeamIndex != KillerPS->Team->TeamIndex)
		{
			if (IsTeamOnDefense(VictimPS->Team->TeamIndex))
			{
				++OffenseKills;
			}
			else
			{
				++DefenseKills;
			}
		}
	}

	if (OffenseKills >= OffenseKillsNeededForPowerUp)
	{
		OffenseKills = 0;

		//go through offense and gift them a powerup
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (IsTeamOnOffense(PS->Team->TeamIndex))
			{
				PS->RemainingBoosts = 1;

				AUTPlayerController* PC = Cast<AUTPlayerController>(PS->GetOwner());
				if (PC)
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), 6, KillerPS);
				}

				continue;
			}
			
			AUTPlayerController* PC = Cast<AUTPlayerController>(PS->GetOwner());
			if (PC)
			{
				PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), 7, KillerPS);
			}
		}
	}

	if (DefenseKills >= DefenseKillsNeededForPowerUp)
	{
		DefenseKills = 0;

		//go through defense and gift them a powerup
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (IsTeamOnDefense(PS->Team->TeamIndex))
			{
				PS->RemainingBoosts = 1;

				AUTPlayerController* PC = Cast<AUTPlayerController>(PS->GetOwner());
				if (PC)
				{
					PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), 6, KillerPS);
				}

				continue;
			}

			AUTPlayerController* PC = Cast<AUTPlayerController>(PS->GetOwner());
			if (PC)
			{
				PC->ClientReceiveLocalizedMessage(UUTCTFRoleMessage::StaticClass(), 7, KillerPS);
			}	
		}
	}

	Super::DiscardInventory(Other, Killer);
}

void AUTCTFRoundGameTommy::RestartPlayer(AController* aPlayer)
{
	Super::RestartPlayer(aPlayer);

	//Force RespawnWaitTime to not increase since default RCTF increases respawn times each death in RestartPlayer
	AUTPlayerState* PS = Cast<AUTPlayerState>(aPlayer->PlayerState);
	if (PS)
	{
		PS->RespawnWaitTime = UnlimitedRespawnWaitTime;
	}
}

bool AUTCTFRoundGameTommy::IsPlayerOnLifeLimitedTeam(AUTPlayerState& PlayerState) const
{
	return IsTeamOnDefense(PlayerState.Team->TeamIndex);
}