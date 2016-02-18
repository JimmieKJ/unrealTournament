// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTOnlineGameSettings.h"
#include "Qos.h"


FUTOnlineSessionSettings::FUTOnlineSessionSettings(bool bIsLAN, bool bIsPresence, int32 MaxNumPlayers)
{
	NumPublicConnections = FMath::Max(MaxNumPlayers, 0);

	NumPrivateConnections = 0;
	bIsLANMatch = bIsLAN;
	bShouldAdvertise = true;
	bAllowJoinInProgress = true;
	bAllowInvites = true;
	bUsesPresence = bIsPresence;
	bAllowJoinViaPresence = true;
	bAllowJoinViaPresenceFriendsOnly = false;

	Set(SETTING_REGION, UQosEvaluator::GetDefaultRegionString(), EOnlineDataAdvertisementType::ViaOnlineService);
}

FUTOnlineSessionSettingsLobby::FUTOnlineSessionSettingsLobby(bool bIsLAN, bool bIsPresence, int32 MaxNumPlayers) :
FUTOnlineSessionSettings(bIsLAN, bIsPresence, MaxNumPlayers)
{
}

FUTOnlineSessionSettingsLobbyPvP::FUTOnlineSessionSettingsLobbyPvP(bool bIsLAN, bool bIsPresence, int32 MaxNumPlayers) :
FUTOnlineSessionSettingsLobby(bIsLAN, bIsPresence, MaxNumPlayers)
{
	FString GameModeStr(UT_GAMEMODE_PVP);
	Set(SETTING_GAMEMODE, GameModeStr, EOnlineDataAdvertisementType::ViaOnlineService);
	Set(SETTING_ISLOBBY, true, EOnlineDataAdvertisementType::ViaOnlineService);
}

FUTOnlineSessionSettingsDedicatedEmpty::FUTOnlineSessionSettingsDedicatedEmpty(bool bIsLAN, bool bIsPresence, int32 MaxNumPlayers) :
	FUTOnlineSessionSettings(bIsLAN, bIsPresence, MaxNumPlayers)
{
	FString GameModeStr(UT_GAMEMODE_EMPTY);
	Set(SETTING_GAMEMODE, GameModeStr, EOnlineDataAdvertisementType::ViaOnlineService);
}