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

bool UUTPlaylistManager::GetPlaylistName(int32 PlaylistId, FString& OutPlaylistName)
{
	for (const FPlaylistItem& PlaylistEntry : Playlist)
	{
		if (PlaylistEntry.PlaylistId == PlaylistId)
		{
			OutPlaylistName = PlaylistEntry.FriendlyName;
			return true;
		}
	}

	return false;

}

bool UUTPlaylistManager::IsPlaylistRanked(int32 PlaylistId)
{
	for (const FPlaylistItem& PlaylistEntry : Playlist)
	{
		if (PlaylistEntry.PlaylistId == PlaylistId)
		{
			return PlaylistEntry.bRanked;
		}
	}

	return false;
}

bool UUTPlaylistManager::GetTeamEloRatingForPlaylist(int32 PlaylistId, FString& TeamEloRating)
{
	for (const FPlaylistItem& PlaylistEntry : Playlist)
	{
		if (PlaylistEntry.PlaylistId == PlaylistId)
		{
			TeamEloRating = PlaylistEntry.TeamEloRating;
			return true;
		}
	}

	return false;
}

bool UUTPlaylistManager::GetGameModeForPlaylist(int32 PlaylistId, FString& GameMode)
{
	for (const FPlaylistItem& PlaylistEntry : Playlist)
	{
		if (PlaylistEntry.PlaylistId == PlaylistId)
		{
			GameMode = PlaylistEntry.GameMode;
			return true;
		}
	}

	return false;
}

bool UUTPlaylistManager::GetURLForPlaylist(int32 PlaylistId, FString& URL)
{
	for (const FPlaylistItem& PlaylistEntry : Playlist)
	{
		if (PlaylistEntry.PlaylistId == PlaylistId)
		{
			URL = PlaylistEntry.MapNames[FMath::RandRange(0, PlaylistEntry.MapNames.Num() - 1)];
			URL += TEXT("?game=") + PlaylistEntry.GameMode;
			URL += PlaylistEntry.ExtraCommandline;

			URL += TEXT("?MatchmakingSession=1");

			if (PlaylistEntry.bRanked)
			{
				URL += TEXT("?Ranked=1");
			}
			else
			{
				URL += TEXT("?QuickMatch=1");
			}

			return true;
		}
	}

	return false;
}

void UUTPlaylistManager::UpdatePlaylistFromMCP(int32 PlaylistId, FString InExtraCommandline, TArray<FString>& InMapNames)
{
	for (FPlaylistItem& PlaylistEntry : Playlist)
	{
		if (PlaylistEntry.PlaylistId == PlaylistId)
		{
			PlaylistEntry.ExtraCommandline = InExtraCommandline;
			PlaylistEntry.MapNames = InMapNames;
		}
	}
}