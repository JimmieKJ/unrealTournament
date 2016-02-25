// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineAsyncTaskManagerSteam.h"
#include "OnlineSubsystemSteamTypes.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemSteamPackage.h"

/** 
 *  Async task for creating a Steam backend lobby as host and defining the proper settings
 */
class FOnlineAsyncTaskSteamCreateLobby : public FOnlineAsyncTaskSteam
{
private:

	/** Has this request been started */
	bool bInit;
	/** Name of session being created */
	FName SessionName;
	/** Type of lobby to create */
	ELobbyType LobbyType;
	/** Max number of players allowed */
	int32 MaxLobbyMembers;

	/** Hidden on purpose */
	FOnlineAsyncTaskSteamCreateLobby() : 
		bInit(false),
		SessionName(NAME_None),
		LobbyType(k_ELobbyTypePublic),
		MaxLobbyMembers(0)
	{
	}

PACKAGE_SCOPE:

	/** Lobby created callback data */
	LobbyCreated_t CallbackResults;

public:

	FOnlineAsyncTaskSteamCreateLobby(class FOnlineSubsystemSteam* InSubsystem, FName InSessionName, ELobbyType InLobbyType, int32 InMaxLobbyMembers) :
		FOnlineAsyncTaskSteam(InSubsystem, k_uAPICallInvalid),
		bInit(false),
		SessionName(InSessionName),
		LobbyType(InLobbyType),
		MaxLobbyMembers(InMaxLobbyMembers)
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() override;
	
	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

/** 
 *  Async task to update a single Steam lobby
 */
class FOnlineAsyncTaskSteamUpdateLobby : public FOnlineAsyncTaskSteam
{
private:

	/** Name of session being created */
	FName SessionName;
	/** New session settings to apply */
	FOnlineSessionSettings NewSessionSettings;
	/** Should the online platform refresh as well */
	bool bUpdateOnlineData;

	/** Hidden on purpose */
	FOnlineAsyncTaskSteamUpdateLobby() : 
		SessionName(NAME_None),
		bUpdateOnlineData(false)
	{
	}

public:

	/** Constructor */
	FOnlineAsyncTaskSteamUpdateLobby(class FOnlineSubsystemSteam* InSubsystem, FName InSessionName, bool bInUpdateOnlineData, const FOnlineSessionSettings& InNewSessionSettings) :
		FOnlineAsyncTaskSteam(InSubsystem, k_uAPICallInvalid),
		SessionName(InSessionName),
		NewSessionSettings(InNewSessionSettings),
		bUpdateOnlineData(bInUpdateOnlineData)
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	* Give the async task time to do its work
	* Can only be called on the async task manager thread
	*/
	virtual void Tick() override;
	
	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

/** 
 *  Async task to join a single Steam lobby
 */
class FOnlineAsyncTaskSteamJoinLobby : public FOnlineAsyncTaskSteam
{
private:

	/** Has this request been started */
	bool bInit;
	/** Name of session being created */
	FName SessionName;
	/** Lobby to join */
	FUniqueNetIdSteam LobbyId;

	/** Hidden on purpose */
	FOnlineAsyncTaskSteamJoinLobby() : 
		bInit(false),
		SessionName(NAME_None),
		LobbyId(0)
	{
	}

PACKAGE_SCOPE:

	/** Join request callback data */
	LobbyEnter_t CallbackResults;

public:

	/** Constructor */
	FOnlineAsyncTaskSteamJoinLobby(class FOnlineSubsystemSteam* InSubsystem, FName InSessionName, const FUniqueNetIdSteam& InLobbyId) :
		FOnlineAsyncTaskSteam(InSubsystem, k_uAPICallInvalid),
		bInit(false),
		SessionName(InSessionName),
		LobbyId(InLobbyId)
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	* Give the async task time to do its work
	* Can only be called on the async task manager thread
	*/
	virtual void Tick() override;

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() override;
	
	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

/** 
 *  Async task for leaving a single lobby
 */
class FOnlineAsyncTaskSteamLeaveLobby : public FOnlineAsyncTaskSteam
{
private:

	/** Name of session lobby */
	FName SessionName;
	/** LobbyId to end */
	FUniqueNetIdSteam LobbyId;

	/** Hidden on purpose */
	FOnlineAsyncTaskSteamLeaveLobby() : 
		SessionName(NAME_None),
		LobbyId(uint64(0))
	{
	}

public:

	FOnlineAsyncTaskSteamLeaveLobby(class FOnlineSubsystemSteam* InSubsystem, FName InSessionName, const FUniqueNetIdSteam& InLobbyId) :
		FOnlineAsyncTaskSteam(InSubsystem, k_uAPICallInvalid),
		SessionName(InSessionName),
		LobbyId(InLobbyId)
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;
};

/** 
 *  Async task for any search query to find Steam lobbies based on search criteria
 */
class FOnlineAsyncTaskSteamFindLobbies : public FOnlineAsyncTaskSteam
{
private:

	/** Has this request been started */
	bool bInit;
	/** Has the lobby request portion been complete */
	bool bLobbyRequestComplete;
	/** Search settings specified for the query */
	TSharedPtr<class FOnlineSessionSearch> SearchSettings;
	/** Cached instance of Steam interface */
	ISteamMatchmaking* SteamMatchmakingPtr;

	/** Hidden on purpose */
	FOnlineAsyncTaskSteamFindLobbies() : 
		bInit(false),
		bLobbyRequestComplete(false),
		SteamMatchmakingPtr(NULL)
	{	
	}

	/**
	 *	Create and trigger the lobby query from the defined search settings 
	 */
	virtual void CreateQuery();

PACKAGE_SCOPE:

	/** Lobby search callback data */
	LobbyMatchList_t CallbackResults;

public:

	/** Constructor */
	FOnlineAsyncTaskSteamFindLobbies(class FOnlineSubsystemSteam* InSubsystem, const TSharedPtr<FOnlineSessionSearch>& InSearchSettings) :
		FOnlineAsyncTaskSteam(InSubsystem, k_uAPICallInvalid),
		bInit(false),
		bLobbyRequestComplete(false),
		SearchSettings(InSearchSettings),
		SteamMatchmakingPtr(SteamMatchmaking())
	{
	}

	/**
	 *	Create a search result from a specified lobby id 
	 *
	 * @param LobbyId lobby to create the search result for
	 */
	void ParseSearchResult(FUniqueNetIdSteam& LobbyId);

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() override;
	
	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

DECLARE_MULTICAST_DELEGATE_FourParams(FOnAsyncFindLobbyCompleteWithNetId, const bool, const int32, TSharedPtr< const FUniqueNetId >, const class FOnlineSessionSearchResult&);
typedef FOnAsyncFindLobbyCompleteWithNetId::FDelegate FOnAsyncFindLobbyCompleteDelegateWithNetId;

/** 
 *  Async task for any search query to find Steam lobbies based on search criteria
 */
class FOnlineAsyncTaskSteamFindLobby : public FOnlineAsyncTaskSteam
{
private:

	/** Has this request been started */
	bool bInit;
	/** Lobby Id to search for */
	FUniqueNetIdSteam LobbyId;
	/** Search settings ignored except to hold the final search result */
	TSharedPtr<class FOnlineSessionSearch> SearchSettings;
	/** User that initiated the request */
	int32 LocalUserNum;
	/** Delegates to fire when the search is complete. Only one of these will be called based on which constructor was used. */
	FOnSingleSessionResultComplete OnFindLobbyCompleteDelegates;
	FOnAsyncFindLobbyCompleteWithNetId OnFindLobbyCompleteWithNetIdDelegate;
	/** This is true if the delegate passed in to the constructor was a FOnAsyncFindLobbyCompleteWithNetId delegate, false otherwise. */
	bool bIsUsingNetIdDelegate;

	/** Cached instance of Steam interface */
	ISteamMatchmaking* SteamMatchmakingPtr;

	/** Hidden on purpose */
	FOnlineAsyncTaskSteamFindLobby() : 
		bInit(false),
		SteamMatchmakingPtr(NULL)
	{	
	}

public:

	/** Constructor */
	FOnlineAsyncTaskSteamFindLobby(class FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InLobbyId, const TSharedPtr<FOnlineSessionSearch>& InSearchSettings, int32 InLocalUserNum, const FOnSingleSessionResultComplete& InOnFindLobbyCompleteDelegates) :
		FOnlineAsyncTaskSteam(InSubsystem, k_uAPICallInvalid),
		bInit(false),
		LobbyId(InLobbyId),
		SearchSettings(InSearchSettings),
		LocalUserNum(InLocalUserNum),
		OnFindLobbyCompleteDelegates(InOnFindLobbyCompleteDelegates),
        bIsUsingNetIdDelegate(false),
        SteamMatchmakingPtr(SteamMatchmaking())
	{
	}

	FOnlineAsyncTaskSteamFindLobby(class FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InLobbyId, const TSharedPtr<FOnlineSessionSearch>& InSearchSettings, int32 InLocalUserNum, const FOnAsyncFindLobbyCompleteWithNetId& InOnFindLobbyCompleteDelegates) :
		FOnlineAsyncTaskSteam(InSubsystem, k_uAPICallInvalid),
		bInit(false),
		LobbyId(InLobbyId),
		SearchSettings(InSearchSettings),
		LocalUserNum(InLocalUserNum),
		OnFindLobbyCompleteWithNetIdDelegate(InOnFindLobbyCompleteDelegates),
		bIsUsingNetIdDelegate(true),
        SteamMatchmakingPtr(SteamMatchmaking())
	{
	}

	/**
	 *	Create a search result from a specified lobby id 
	 *
	 * @param LobbyId lobby to create the search result for
	 */
	void ParseSearchResult(FUniqueNetIdSteam& LobbyId);

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskSteamFindLobby bWasSuccessful: %d LobbyId: %s"), bWasSuccessful, *LobbyId.ToDebugString());
	}

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() override;
	
	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

/**
 *	Turns a friends accepted invite request into a valid search result (lobby version)
 */
class FOnlineAsyncEventSteamLobbyInviteAccepted : public FOnlineAsyncEvent<FOnlineSubsystemSteam>
{
	/** Friend that invited */
	FUniqueNetIdSteam FriendId;
	/** Lobby to go to */
	FUniqueNetIdSteam LobbyId;
	/** User initiating the request */
	int32 LocalUserNum;

	/** Hidden on purpose */
	FOnlineAsyncEventSteamLobbyInviteAccepted() :
		FOnlineAsyncEvent(NULL),
		FriendId((uint64)0),
		LobbyId((uint64)0)
	{
	}

public:
	FOnlineAsyncEventSteamLobbyInviteAccepted(FOnlineSubsystemSteam* InSubsystem, const FUniqueNetIdSteam& InFriendId, const FUniqueNetIdSteam& InLobbyId) :
		FOnlineAsyncEvent(InSubsystem),
		FriendId(InFriendId),
		LobbyId(InLobbyId),
		LocalUserNum(0)
	{
	}
	
	virtual ~FOnlineAsyncEventSteamLobbyInviteAccepted() 
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() override;
};

/**
 *	Generate the proper lobby type from session settings
 *
 * @param SessionSettings current settings for the session
 *
 * @return type of lobby to generate, defaulting to private if not advertising and public otherwise
 */
ELobbyType BuildLobbyType(FOnlineSessionSettings* SessionSettings);

/**
 *	Populate an FSession data structure from the data stored with a lobby
 * Expects a certain number of keys to be present otherwise this will fail
 *
 * @param LobbyId the Steam lobby to fill data from
 * @param Session empty session structure to fill in
 *
 * @return true if successful, false otherwise
 */
bool FillSessionFromLobbyData(FUniqueNetIdSteam& LobbyId, class FOnlineSession& Session);

/**
 *	Populate an FSession data structure from the data stored with the members of the lobby
 *
 * @param LobbyId the Steam lobby to fill data from
 * @param Session session structure to fill in
 *
 * @return true if successful, false otherwise
 */
bool FillMembersFromLobbyData(FUniqueNetIdSteam& LobbyId, class FNamedOnlineSession& Session);
