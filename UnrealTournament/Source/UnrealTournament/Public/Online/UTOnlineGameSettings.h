// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Setting describing the game mode of the session (value is int32) */
#define SETTING_PLAYLISTID FName(TEXT("PLAYLISTID"))

/** String describing if the game is in lobby mode (value is bool) */
#define SETTING_ISLOBBY		FName(TEXT("ISLOBBY"))

#define UT_GAMEMODE_PVP		TEXT("UTPVP")

/**
 * General session settings for a Fortnite game
 */
class UNREALTOURNAMENT_API FUTOnlineSessionSettings : public FOnlineSessionSettings
{
public:

	FUTOnlineSessionSettings(bool bIsLAN = false, bool bIsPresence = false, int32 MaxNumPlayers = 4);
	virtual ~FUTOnlineSessionSettings() {}

	/** Returns the configured region */
	static FString GetRegionString();
};

/**
 * General session settings for a Fortnite lobby game
 */
class UNREALTOURNAMENT_API FUTOnlineSessionSettingsLobby : public FUTOnlineSessionSettings
{
public:

	FUTOnlineSessionSettingsLobby(bool bIsLAN = false, bool bIsPresence = false, int32 MaxNumPlayers = 4);
	virtual ~FUTOnlineSessionSettingsLobby() {}
};

/**
 * General session settings for a Fortnite pvp lobby game
 */
class UNREALTOURNAMENT_API FUTOnlineSessionSettingsLobbyPvP : public FUTOnlineSessionSettingsLobby
{
public:

	FUTOnlineSessionSettingsLobbyPvP(bool bIsLAN = false, bool bIsPresence = false, int32 MaxNumPlayers = 4);
	virtual ~FUTOnlineSessionSettingsLobbyPvP() {}
};

/**
 * Base search settings for a Fortnite game
 */
class UNREALTOURNAMENT_API FUTOnlineSessionSearchBase : public FOnlineSessionSearch
{
protected:

	/** Index of game mode requested */
	int32 PlaylistId;

	FUTOnlineSessionSearchBase();

public:

	FUTOnlineSessionSearchBase(int32 InPlaylistId, bool bSearchingLAN = false, bool bSearchingPresence = false);
	virtual ~FUTOnlineSessionSearchBase() {}

	/** @return the playlist id query param */
	int32 GetPlaylistId() const { return PlaylistId; }
};