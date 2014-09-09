// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "OnlineSessionSettings.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTGameEngine.h"

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

	Set(SETTING_GAMEMODE, CurrentGame->FriendlyGameName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Set(SETTING_MAPNAME, CurrentGame->GetWorld()->GetMapName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	if (CurrentGame->UTGameState)
	{
		Set(SETTING_SERVERNAME, CurrentGame->UTGameState->ServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		Set(SETTING_SERVERMOTD, CurrentGame->UTGameState->ServerMOTD, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}

	Set(SETTING_PLAYERSONLINE, CurrentGame->NumPlayers, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Set(SETTING_SPECTATORSONLINE, CurrentGame->NumSpectators, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		
	FString GameVer = FString::Printf(TEXT("%i"),GetDefault<UUTGameEngine>()->GameNetworkVersion);
	Set(SETTING_SERVERVERSION, GameVer, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
}

