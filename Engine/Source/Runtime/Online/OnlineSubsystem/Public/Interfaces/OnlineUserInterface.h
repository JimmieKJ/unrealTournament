// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineDelegateMacros.h"

/**
 * Delegate used when the user query request has completed
 *
 * @param LocalUserNum the controller number of the associated user that made the request
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 * @param UserIds list of user ids that were queried
 * @param ErrorStr string representing the error condition
 */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnQueryUserInfoComplete, int32, bool, const TArray< TSharedRef<class FUniqueNetId> >&, const FString&);
typedef FOnQueryUserInfoComplete::FDelegate FOnQueryUserInfoCompleteDelegate;

/**
 *	Interface class for obtaining online User info
 */
class IOnlineUser
{

public:

	/**
	 * Starts an async task that queries/reads the info for a list of users
	 *
	 * @param LocalUserNum the user requesting the query
	 * @param UserIds list of users to read info about
	 *
	 * @return true if the read request was started successfully, false otherwise
	 */
	virtual bool QueryUserInfo(int32 LocalUserNum, const TArray<TSharedRef<class FUniqueNetId> >& UserIds) = 0;

	/**
	 * Delegate used when the user query request has completed
	 *
	 * @param LocalUserNum the controller number of the associated user that made the request
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 * @param UserIds list of user ids that were queried
	 * @param ErrorStr string representing the error condition
	 */
	DEFINE_ONLINE_PLAYER_DELEGATE_THREE_PARAM(MAX_LOCAL_PLAYERS, OnQueryUserInfoComplete, bool, const TArray< TSharedRef<class FUniqueNetId> >&, const FString&);

	/**
	 * Obtains the cached list of online user info 
	 *
	 * @param LocalUserNum the local user that queried for online user data
	 * @param OutUsers [out] array that receives the copied data
	 *
	 * @return true if user info was found
	 */
	virtual bool GetAllUserInfo(int32 LocalUserNum, TArray< TSharedRef<class FOnlineUser> >& OutUsers) = 0;

	/**
	 * Get the cached user entry for a specific user id if found
	 *
	 * @param LocalUserNum the local user that queried for online user data
	 * @param UserId id to use for finding the cached online user
	 *
	 * @return user info or null ptr if not found
	 */
	virtual TSharedPtr<FOnlineUser> GetUserInfo(int32 LocalUserNum, const class FUniqueNetId& UserId) = 0;
};
