// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "OnlineSessionSettings.h"
#include "UTOnlineGameSettingsBase.h"

FUTOnlineGameSettingsBase::FUTOnlineGameSettingsBase(bool bIsLanGame, bool bIsPresense, int32 MaxNumberPlayers)
{
	NumPublicConnections = FMath::Max(MaxNumberPlayers, 0);
	NumPrivateConnections = 0;
	bIsLANMatch = bIsLanGame;

	bShouldAdvertise = true;
	bAllowJoinInProgress = true;
	bAllowInvites = true;
	bUsesPresence = bIsPresense;
	bAllowJoinViaPresence = bIsPresense;
	bAllowJoinViaPresenceFriendsOnly = bIsPresense;
}

void FUTOnlineGameSettingsBase::ApplyGameSettings(AUTGameMode* CurrentGame)
{
	// Stub function.  We will need to fill this out later.
	bIsDedicated = CurrentGame->GetWorld()->GetNetMode() == NM_DedicatedServer;

	Set(SETTING_GAMEMODE, GetNameSafe(CurrentGame), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Set(SETTING_MAPNAME, CurrentGame->GetWorld()->GetMapName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	if (CurrentGame->UTGameState)
	{
		Set(SETTING_SERVERNAME, CurrentGame->UTGameState->ServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		Set(SETTING_MOTD, CurrentGame->UTGameState->ServerMOTD, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}

	UpdateGameSettings(CurrentGame);
}

void FUTOnlineGameSettingsBase::UpdateGameSettings(AUTGameMode* CurrentGame)
{
	Set(SETTING_PLAYERSONLINE, CurrentGame->NumPlayers, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Set(SETTING_SPECTATORSONLINE, CurrentGame->NumSpectators, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
}