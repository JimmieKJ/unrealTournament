// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPlaylistManager.h"

bool UUTPlaylistManager::GetMaxTeamInfoForPlaylist(int32 PlaylistId, int32& MaxTeamCount, int32& MaxTeamSize, int32& MaxPartySize)
{
	for (const FPlaylistItem& PlaylistEntry : Playlist)
	{
		if (PlaylistEntry.PlaylistId == PlaylistId)
		{
			MaxTeamCount = PlaylistEntry.MaxTeamCount;
			MaxTeamSize = PlaylistEntry.MaxTeamSize;
			MaxPartySize = PlaylistEntry.MaxPartySize;
			return true;
		}
	}

	return false;
}

bool  UUTPlaylistManager::GetURLForPlaylist(int32 PlaylistId, FString& URL)
{
	for (const FPlaylistItem& PlaylistEntry : Playlist)
	{
		if (PlaylistEntry.PlaylistId == PlaylistId)
		{
			URL = PlaylistEntry.MapNames[FMath::RandRange(0, PlaylistEntry.MapNames.Num() - 1)];
			URL += TEXT("?game=") + PlaylistEntry.GameMode;
			URL += PlaylistEntry.ExtraCommandline;
			URL += TEXT("?Ranked=1");
		}
	}

	return false;
}