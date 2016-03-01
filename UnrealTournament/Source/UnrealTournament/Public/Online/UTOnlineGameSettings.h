// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Setting describing the game mode of the session (value is int32) */
#define SETTING_PLAYLISTID FName(TEXT("PLAYLISTID"))

/** Setting describing the elo of the session (value is int32) */
#define SETTING_TEAMELO FName(TEXT("TEAMELO"))

/** Setting describing the trust level of the session (value is FString) */
#define SETTING_TRUSTLEVEL FName(TEXT("UT_SERVERTRUSTLEVEL"))

#define GAMEMODE_EMPTY	TEXT("EMPTY")

/** Named interface reference for storing beacon state between levels */
#define UT_BEACON_STATE FName(TEXT("PartyBeaconState"))

#define UT_DEFAULT_MAX_TEAM_COUNT 2
#define UT_DEFAULT_MAX_TEAM_SIZE 5
#define UT_DEFAULT_PARTY_SIZE 5

/** Number of players needed to fill out this session (value is int32) */
#define SETTING_NEEDS FName(TEXT("NEEDS"))

/** Second key for "needs" because can't set same value with two criteria (value is int32) */
#define SETTING_NEEDSSORT FName(TEXT("NEEDSSORT"))

/** Value to sort results in ascending order given a "distance" sort criteria */
#define SORT_ASC MIN_int32
/** Value to sort results in descending order given a "distance" sort criteria */
#define SORC_DESC MAX_int32

struct FEmptyServerReservation;

/**
 * General session settings for a UT game
 */
class UNREALTOURNAMENT_API FUTOnlineSessionSettings : public FOnlineSessionSettings
{
public:

	FUTOnlineSessionSettings(bool bIsLAN = false, bool bIsPresence = false, int32 MaxNumPlayers = 10);
	virtual ~FUTOnlineSessionSettings() {}

	/** Returns the configured region */
	static FString GetRegionString();
};

/**
* General session settings for a UT dedicated game
*/
class UNREALTOURNAMENT_API FUTOnlineSessionSettingsDedicatedEmpty : public FUTOnlineSessionSettings
{
public:

	FUTOnlineSessionSettingsDedicatedEmpty(bool bIsLAN = false, bool bIsPresence = false, int32 MaxNumPlayers = 10);
	virtual ~FUTOnlineSessionSettingsDedicatedEmpty() {}
};

/**
 * Base search settings for a UT game
 */
class UNREALTOURNAMENT_API FUTOnlineSessionSearchBase : public FOnlineSessionSearch
{
protected:

	/** Index of game mode requested */
	int32 PlaylistId;

	/** Elo of game requested */
	int32 TeamElo;

	FUTOnlineSessionSearchBase();

public:

	FUTOnlineSessionSearchBase(int32 InPlaylistId, int32 InTeamElo, bool bSearchingLAN = false, bool bSearchingPresence = false);
	virtual ~FUTOnlineSessionSearchBase() {}

	/** @return the playlist id query param */
	int32 GetPlaylistId() const { return PlaylistId; }

	int32 GetTeamElo() const { return TeamElo; }
};

/**
 * Search settings for a session that is gathering players 
 */
class UNREALTOURNAMENT_API FUTOnlineSessionSearchGather : public FUTOnlineSessionSearchBase
{
public:
	FUTOnlineSessionSearchGather(int32 InPlaylistId = INDEX_NONE, bool bSearchingLAN = false, bool bSearchingPresence = false);
	virtual ~FUTOnlineSessionSearchGather() {}

	// FOnlineSessionSearch Interface begin
	virtual TSharedPtr<FOnlineSessionSettings> GetDefaultSessionSettings() const override { return MakeShareable(new FUTOnlineSessionSettings()); }
	virtual void SortSearchResults() override;
	// FOnlineSessionSearch Interface end
};

/**
 * Search settings for an empty dedicated server to host a match
 */
class UNREALTOURNAMENT_API FUTOnlineSessionSearchEmptyDedicated : public FUTOnlineSessionSearchBase
{
protected:
	FUTOnlineSessionSearchEmptyDedicated();

public:
	FUTOnlineSessionSearchEmptyDedicated(const FEmptyServerReservation& InReservationData, bool bSearchingLAN = false, bool bSearchingPresence = false);
	virtual ~FUTOnlineSessionSearchEmptyDedicated() {}

	// FOnlineSessionSearch Interface begin
	virtual TSharedPtr<FOnlineSessionSettings> GetDefaultSessionSettings() const override { return MakeShareable(new FUTOnlineSessionSettingsDedicatedEmpty()); }
	// FOnlineSessionSearch Interface end
};
