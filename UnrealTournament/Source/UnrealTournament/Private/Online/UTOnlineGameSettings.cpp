// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTOnlineGameSettings.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTPartyBeaconState.h"
#include "QosInterface.h"

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
	
	FString Region = FQosInterface::GetDefaultRegionString();
	FString RegionOverride;
	if (FParse::Value(FCommandLine::Get(), TEXT("REGION="), RegionOverride))
	{
		Region = RegionOverride;
	}

	Set(SETTING_REGION, Region, EOnlineDataAdvertisementType::ViaOnlineService);

	Set(SETTING_RANKED, 1, EOnlineDataAdvertisementType::ViaOnlineService);
}

FUTOnlineSessionSettingsDedicatedEmpty::FUTOnlineSessionSettingsDedicatedEmpty(bool bIsLAN, bool bIsPresence, int32 MaxNumPlayers) :
	FUTOnlineSessionSettings(bIsLAN, bIsPresence, MaxNumPlayers)
{
	bAllowInvites = false;
	bAllowJoinViaPresence = true;
	bAllowJoinViaPresenceFriendsOnly = false;

	FString GameModeStr(GAMEMODE_EMPTY);
	Set(SETTING_GAMEMODE, GameModeStr, EOnlineDataAdvertisementType::ViaOnlineService);
}

FUTOnlineSessionSearchGather::FUTOnlineSessionSearchGather(int32 InPlaylistId, int32 InTeamElo, bool bInRanked, bool bSearchingLAN, bool bSearchingPresence, FString InServerToSkip) :
	FUTOnlineSessionSearchBase(InPlaylistId, InTeamElo, bInRanked, bSearchingLAN, bSearchingPresence)
{
	MaxSearchResults = 20;
	ServerToSkip = InServerToSkip;

	// Only find Epic hosted servers
	QuerySettings.Set(SETTING_TRUSTLEVEL, 0, EOnlineComparisonOp::Equals);
}

void FUTOnlineSessionSearchGather::SortSearchResults()
{
	// Currently no custom sorting
	if (!ServerToSkip.IsEmpty())
	{
		for (int i = SearchResults.Num() - 1; i >= 0; i--)
		{
			if (SearchResults[i].Session.SessionInfo.IsValid())
			{
				if (SearchResults[i].Session.SessionInfo->GetSessionId().ToString() == ServerToSkip)
				{
					SearchResults.RemoveAt(i);
				}
			}
		}
	}
}

FUTOnlineSessionSearchEmptyDedicated::FUTOnlineSessionSearchEmptyDedicated()
{
	MaxSearchResults = 20;
}
	
FUTOnlineSessionSearchEmptyDedicated::FUTOnlineSessionSearchEmptyDedicated(const FEmptyServerReservation& InReservationData, bool bSearchingLAN, bool bSearchingPresence) :
	FUTOnlineSessionSearchBase(INDEX_NONE, InReservationData.TeamElo, InReservationData.bRanked, bSearchingLAN, bSearchingPresence)
{
	MaxSearchResults = 20;

	QuerySettings.Set(SEARCH_DEDICATED_ONLY, true, EOnlineComparisonOp::Equals);
	QuerySettings.Set(SEARCH_EMPTY_SERVERS_ONLY, true, EOnlineComparisonOp::Equals);

	FString GameModeStr(GAMEMODE_EMPTY);
	QuerySettings.Set(SETTING_GAMEMODE, GameModeStr, EOnlineComparisonOp::Equals);

	// Only find Epic hosted servers
	QuerySettings.Set(SETTING_TRUSTLEVEL, 0, EOnlineComparisonOp::Equals);

	PlaylistId = InReservationData.PlaylistId;
	TeamElo = InReservationData.TeamElo;
	bRanked = InReservationData.bRanked;
}

FUTOnlineSessionSearchBase::FUTOnlineSessionSearchBase()
{
	bIsLanQuery = false;
}

FUTOnlineSessionSearchBase::FUTOnlineSessionSearchBase(int32 InPlaylistId, int32 InTeamElo, bool bInRanked, bool bSearchingLAN, bool bSearchingPresence)
{
	bIsLanQuery = bSearchingLAN;

	PlaylistId = InPlaylistId;
	TeamElo = InTeamElo;
	bRanked = bInRanked;

	QuerySettings.Set(SEARCH_MINSLOTSAVAILABLE, 1, EOnlineComparisonOp::Equals);

	if (PlaylistId != INDEX_NONE)
	{
		QuerySettings.Set(SETTING_PLAYLISTID, PlaylistId, EOnlineComparisonOp::Equals);
	}
}