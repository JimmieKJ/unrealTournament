// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPlaylistManager.generated.h"

USTRUCT()
struct FPlaylistItem
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString FriendlyName;
	UPROPERTY()
	int32 PlaylistId;
	UPROPERTY()
	bool bRanked;
	UPROPERTY()
	int32 MaxTeamCount;
	UPROPERTY()
	int32 MaxTeamSize;
	UPROPERTY()
	int32 MaxPartySize;
	UPROPERTY()
	FString GameMode;
	UPROPERTY()
	FString ExtraCommandline;
	UPROPERTY()
	FString TeamEloRating;
	UPROPERTY()
	TArray<FString> MapNames;
	UPROPERTY()
	FString SlateBadgeName;
};

UCLASS(config = Game, notplaceable)
class UNREALTOURNAMENT_API UUTPlaylistManager : public UObject
{
	GENERATED_BODY()

		UPROPERTY(Config)
		TArray<FPlaylistItem> Playlist;

public:
	/**
	 * Get the largest team count and team size for any zoneids on the specified playlist
	 *
	 * @param PlaylistId playlist to query
	 * @param MaxTeamCount largest number of possible teams in the game
	 * @param MaxTeamSize largest number of possible players on a team
	 * @param MaxPartySize maximum number of players that can join as a single party
	 */
	bool GetMaxTeamInfoForPlaylist(int32 PlaylistId, int32& MaxTeamCount, int32& MaxTeamSize, int32& MaxPartySize);

	bool GetURLForPlaylist(int32 PlaylistId, FString& URL);

	bool GetTeamEloRatingForPlaylist(int32 PlaylistId, FString& TeamEloRating);

	void UpdatePlaylistFromMCP(int32 PlaylistId, FString InExtraCommandline, TArray<FString>& InMapNames);

	int32 GetNumPlaylists() { return Playlist.Num(); }

	bool GetPlaylistId(int32 PlaylistIndex, int32& PlaylistId);

	bool GetPlaylistName(int32 PlaylistId, FString& OutPlaylistName);

	FName GetPlaylistSlateBadge(int32 PlaylistId);


	bool GetGameModeForPlaylist(int32 PlaylistId, FString& GameMode);

	bool IsPlaylistRanked(int32 PlaylistId);
};