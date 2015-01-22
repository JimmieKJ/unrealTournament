// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSessionSettings.h"
#include "OnlineSubsystemTypes.h"

/**
* Delegate used in friends list change notifications
*/
DECLARE_MULTICAST_DELEGATE(FOnPartyChange);
typedef FOnPartyChange::FDelegate FOnPartyChangeDelegate;

/**
 * Interface definition for the online party services
 */
class IOnlineParty
{
protected:

	/** Hidden on purpose */
	IOnlineParty() {};

public:
	/**
	 * Fill in an array with the current party members
	 * 
	 * @param LocalUserNum the index of the local user making the request
	 * @param PartyMembers the array to receive the party member data
	 */
	virtual EOnlineAsyncTaskState::Type GetMembers(int32 LocalUserNum, TArray<FOnlinePartyMember>& PartyMembers) = 0;

	/** Returns the number of members currently in the party */
	virtual int32 GetNumberOfMembers() = 0;

	/**
     * Delegate used in party change notifications
     */
	DEFINE_ONLINE_PLAYER_DELEGATE(MAX_LOCAL_PLAYERS, OnPartyChange);

	/**
	 * Sends an invite to a list of players to join the party.
	 *
	 * @param LocalUserNum the index of the local user sending the invitation
	 * @param Players the players to invite
	 */
	virtual void InvitePlayersToParty(int32 LocalUserNum, const TArray<TSharedRef<FUniqueNetId>>& Players) = 0;

	/** Returns true if the user represented by LocalUserNum is currently in the party */
	virtual bool IsInParty(int32 LocalUserNum) = 0;

	/**
	 * Delegate executed when the local player operation completes.
	 *
	 * @param bWasSuccessful whether the operation succeeded or not
	 */
	DECLARE_DELEGATE_OneParam(FOnLocalUserOperationCompleteDelegate, bool);

	/**
	 * Adds local users to the party.
	 *
	 * @param ActingUser the id of a user who's already signed in
	 * @param UsersToAdd the ids if the new users to add to the party
	 * @param Delegate the delegate to execute when the async operation completes
	 */
	virtual void AddLocalUsers(
		const FUniqueNetId& ActingUser,
		const TArray<TSharedRef<FUniqueNetId>>& UsersToAdd,
		const FOnLocalUserOperationCompleteDelegate& Delegate) = 0;

	/**
	 * Removes local users from the party.
	 *
	 * @param ActingUser the id of a user who's already signed in
	 * @param UsersToAdd the ids if the new users to add to the party
	 * @param Delegate the delegate to execute when the async operation completes
	 */
	virtual void RemoveLocalUsers(
		const TArray<TSharedRef<FUniqueNetId>>& UsersToRemove,
		const FOnLocalUserOperationCompleteDelegate& Delegate) = 0;
};