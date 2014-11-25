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

void FUTOnlineGameSettingsBase::ApplyGameSettings(AUTBaseGameMode* CurrentGame)
{
	if (!CurrentGame) return;


	// Stub function.  We will need to fill this out later.
	bIsDedicated = CurrentGame->GetWorld()->GetNetMode() == NM_DedicatedServer;

	AUTGameMode* UTGameMode = Cast<AUTGameMode>(CurrentGame);

	Set(SETTING_GAMEMODE, CurrentGame->GetClass()->GetPathName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Set(SETTING_MAPNAME, CurrentGame->GetWorld()->GetMapName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);


	if (UTGameMode)
	{
		Set(SETTING_GAMENAME, UTGameMode->DisplayName.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		if (UTGameMode->UTGameState)
		{
			Set(SETTING_SERVERNAME, UTGameMode->UTGameState->ServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
			Set(SETTING_SERVERMOTD, UTGameMode->UTGameState->ServerMOTD, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		}
	}

	Set(SETTING_PLAYERSONLINE, CurrentGame->NumPlayers, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Set(SETTING_SPECTATORSONLINE, CurrentGame->NumSpectators, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		
	FString GameVer = FString::Printf(TEXT("%i"),GetDefault<UUTGameEngine>()->GameNetworkVersion);
	Set(SETTING_SERVERVERSION, GameVer, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	Set(SETTING_SERVERINSTANCEGUID, CurrentGame->ServerInstanceGUID.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	int32 bGameInstanceServer = CurrentGame->IsGameInstanceServer();
	Set(SETTING_GAMEINSTANCE, bGameInstanceServer, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	int32 ServerFlags = 0x0;
	if (CurrentGame->bRequirePassword) ServerFlags = ServerFlags | SERVERFLAG_RequiresPassword;			// Passworded
}

