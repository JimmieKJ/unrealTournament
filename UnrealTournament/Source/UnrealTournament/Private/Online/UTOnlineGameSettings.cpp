// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTOnlineGameSettings.h"
#include "Qos.h"
#include "UTPartyBeaconState.h"

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

FUTOnlineSessionSearchGather::FUTOnlineSessionSearchGather(int32 InPlaylistId, bool bSearchingLAN, bool bSearchingPresence) :
	FUTOnlineSessionSearchBase(InPlaylistId, bSearchingLAN, bSearchingPresence)
{
}

void FUTOnlineSessionSearchGather::SortSearchResults()
{
	// Currently no custom sorting
}

FUTOnlineSessionSearchEmptyDedicated::FUTOnlineSessionSearchEmptyDedicated()
{
}
	
FUTOnlineSessionSearchEmptyDedicated::FUTOnlineSessionSearchEmptyDedicated(const FEmptyServerReservation& InReservationData, bool bSearchingLAN, bool bSearchingPresence) :
	FUTOnlineSessionSearchBase(InReservationData.PlaylistId, bSearchingLAN, bSearchingPresence)
{
	QuerySettings.Set(SEARCH_DEDICATED_ONLY, true, EOnlineComparisonOp::Equals);
	QuerySettings.Set(SEARCH_EMPTY_SERVERS_ONLY, true, EOnlineComparisonOp::Equals);

	FString GameModeStr(UT_GAMEMODE_EMPTY);
	QuerySettings.Set(SETTING_GAMEMODE, GameModeStr, EOnlineComparisonOp::Equals);
}

FUTOnlineSessionSearchBase::FUTOnlineSessionSearchBase()
{
}

FUTOnlineSessionSearchBase::FUTOnlineSessionSearchBase(int32 InPlaylistId, bool bSearchingLAN, bool bSearchingPresence)
{
	QuerySettings.Set(SEARCH_MINSLOTSAVAILABLE, 1, EOnlineComparisonOp::Equals);

	if (PlaylistId != INDEX_NONE)
	{
		QuerySettings.Set(SETTING_PLAYLISTID, PlaylistId, EOnlineComparisonOp::Equals);
	}
}