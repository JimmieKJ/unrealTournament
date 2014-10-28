// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUD_TeamDM.h"
#include "UTTimedPowerup.h"
#include "UTPickupWeapon.h"
#include "UTWeap_Redeemer.h"
#include "UTDuelGame.h"


AUTDuelGame::AUTDuelGame(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	HUDClass = AUTHUD_DM::StaticClass();

	HUDClass = AUTHUD_TeamDM::StaticClass();
	DisplayName = NSLOCTEXT("UTGameMode", "Duel", "Duel");
	PowerupDuration = 10.f;
	GoalScore = 15;
	TimeLimit = 0.f;
	bPlayersMustBeReady = true;
	bForceRespawn = true;
}

void AUTDuelGame::InitGameState()
{
	Super::InitGameState();

	UTGameState = Cast<AUTGameState>(GameState);
	if (UTGameState != NULL)
	{
		UTGameState->bWeaponStay = false;
	}
}

bool AUTDuelGame::CheckRelevance_Implementation(AActor* Other)
{
	AUTTimedPowerup* Powerup = Cast<AUTTimedPowerup>(Other);
	if (Powerup)
	{
		Powerup->TimeRemaining = PowerupDuration;
	}
	
	// @TODO FIXMESTEVE - don't check for redeemer - once have deployable base class, remove all deployables from duel
	AUTPickupWeapon *PickupWeapon = Cast<AUTPickupWeapon>(Other);
	if (PickupWeapon && (PickupWeapon->WeaponType == AUTWeap_Redeemer::StaticClass()))
	{
		PickupWeapon->WeaponType = nullptr;
	}

	return Super::CheckRelevance_Implementation(Other);
}

void AUTDuelGame::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	GameSession->MaxPlayers = 2;
}

void AUTDuelGame::PlayEndOfMatchMessage()
{
	// individual winner, not team
	AUTGameMode::PlayEndOfMatchMessage();
}