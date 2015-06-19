// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineDelegateMacros.h"

/** Type of presence keys */
typedef FString FPresenceKey;

/** Type of presence properties - a key/value map */
typedef FOnlineKeyValuePairs<FPresenceKey, FVariantData> FPresenceProperties;

/** The default key that will update presence text in the platform's UI */
const FString DefaultPresenceKey = "RichPresence";

/** Custom presence data that is not seen by users but can be polled */
const FString CustomPresenceDataKey = "CustomData";

/** Id of the client that sent the presence update */
const FString DefaultClientIdKey = "ClientId";

/** Id of the session for the presence update. @todo samz - SessionId on presence data should be FUniqueNetId not uint64 */
const FString DefaultSessionIdKey = "SessionId";

namespace EOnlinePresenceState
{
	enum Type : uint8
	{
		Online,
		Offline,
		Away,
		ExtendedAway,
		DoNotDisturb,
		Chat
	};

	/** 
	 * @return the stringified version of the enum passed in 
	 */
	inline const TCHAR* ToString(EOnlinePresenceState::Type EnumVal)
	{
		switch (EnumVal)
		{
		case Online:
			return TEXT("Online");
		case Offline:
			return TEXT("Offline");
		case Away:
			return TEXT("Away");
		case ExtendedAway:
			return TEXT("ExtendedAway");
		case DoNotDisturb:
			return TEXT("DoNotDisturb");
		case Chat:
			return TEXT("Chat");
		}
		return TEXT("");
	}
}

class FOnlineUserPresenceStatus
{
public:
	FString StatusStr;
	EOnlinePresenceState::Type State;
	FPresenceProperties Properties;

	FOnlineUserPresenceStatus()
		: State(EOnlinePresenceState::Offline)
	{

	}
};

/**
 * Presence info for an online user returned via IOnlinePresence interface
 */
class FOnlineUserPresence
{
public:
	TSharedPtr<FUniqueNetId> SessionId;
	uint32 bIsOnline:1;
	uint32 bIsPlaying:1;
	uint32 bIsPlayingThisGame:1;
	uint32 bIsJoinable:1;
	uint32 bHasVoiceSupport:1;
	FOnlineUserPresenceStatus Status;

	/** Constructor */
	FOnlineUserPresence()
	{
		Reset();
	}

	void Reset()
	{
		SessionId = nullptr;
		bIsOnline = 0;
		bIsPlaying = 0;
		bIsPlayingThisGame = 0;
		bIsJoinable = 0;
		bHasVoiceSupport = 0;
		Status = FOnlineUserPresenceStatus();
	}
};

/**
 * Delegate executed when new presence data is available for a user.
 *
 * @param UserId The unique id of the user whose presence was received.
 * @param Presence The unique id of the user whose presence was received.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPresenceReceived, const class FUniqueNetId& /*UserId*/, const TSharedRef<FOnlineUserPresence>& /*Presence*/);
typedef FOnPresenceReceived::FDelegate FOnPresenceReceivedDelegate;

/**
 *	Interface class for getting and setting rich presence information.
 */
class IOnlinePresence
{
public:
	/** Virtual destructor to force proper child cleanup */
	virtual ~IOnlinePresence() {}

	/**
	 * Delegate executed when setting or querying presence for a user has completed.
	 *
	 * @param UserId The unique id of the user whose presence was set.
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error.
	 */
	DECLARE_DELEGATE_TwoParams(FOnPresenceTaskCompleteDelegate, const class FUniqueNetId& /*UserId*/, const bool /*bWasSuccessful*/);

	/**
	 * Starts an async task that sets presence information for the user.
	 *
	 * @param User The unique id of the user whose presence is being set.
	 * @param Status The collection of state and key/value pairs to set as the user's presence data.
	 * @param Delegate The delegate to be executed when the potentially asynchronous set operation completes.
	 */
	virtual void SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) = 0;

	/**
	 * Starts an async operation that will update the cache with presence data from all users in the Users array.
	 * On platforms that support multiple keys, this function will query all keys.
	 *
	 * @param Users The list of unique ids of the users to query for presence information.
	 * @param Delegate The delegate to be executed when the potentially asynchronous query operation completes.
	 */
	//@todo samz - interface should be QueryPresence(const FUniqueNetId& User,  const TArray<TSharedRef<class FUniqueNetId> >& UserIds, const FOnPresenceTaskCompleteDelegate& Delegate)
	virtual void QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) = 0;

	/**
	 * Delegate executed when new presence data is available for a user.
	 *
	 * @param UserId The unique id of the user whose presence was received.
	 * @param Presence The unique id of the user whose presence was received.
	 */
	DEFINE_ONLINE_DELEGATE_TWO_PARAM(OnPresenceReceived, const class FUniqueNetId& /*UserId*/, const TSharedRef<FOnlineUserPresence>& /*Presence*/);

	/**
	 * Gets the cached presence information for a user.
	 *
	 * @param User The unique id of the user from which to get presence information.
	 * @param OutPresence If found, a shared pointer to the cached presence data for User will be stored here.
	 * @return Whether the data was found or not.
	 */
	virtual EOnlineCachedResult::Type GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence) = 0;
};
