// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineFriendsInterface.h"
#include "OnlinePresenceInterface.h"
#include "OnlineSubsystemOculusTypes.h"
#include "OnlineSubsystemOculusPackage.h"
#include <string>

class FOnlineOculusFriend : public FOnlineFriend
{
private:

	TSharedRef<const FUniqueNetIdOculus> UserId;
	const FString DisplayName;
	FOnlineUserPresence Presence;

	std::string InviteToken;

public:

	FOnlineOculusFriend(const ovrID ID, const FString& InDisplayName, ovrUserPresenceStatus FriendPresenceStatus, const char* InInviteToken) :
		UserId(new FUniqueNetIdOculus(ID)),
		DisplayName(InDisplayName),
		InviteToken((InInviteToken != nullptr) ? std::string(InInviteToken) : "")
	{
		Presence.bIsOnline = FriendPresenceStatus == ovrUserPresenceStatus_Online;
	}

	virtual TSharedRef<const FUniqueNetId> GetUserId() const override
	{
		return UserId;
	}

	virtual FString GetRealName() const override
	{
		return TEXT("");
	}

	virtual FString GetDisplayName(const FString& Platform = FString()) const override
	{
		return DisplayName;
	}

	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override
	{
		return false;
	}

	virtual EInviteStatus::Type GetInviteStatus() const override
	{
		return EInviteStatus::Accepted;
	}

	virtual const FOnlineUserPresence& GetPresence() const override
	{
		return Presence;
	}

	const char* GetInviteToken() const
	{
		return InviteToken.c_str();
	}
};

/**
*	IOnlineFriends - Interface class for Friends
*/
class FOnlineFriendsOculus : public IOnlineFriends
{
private:
	
	/** Reference to the owning subsystem */
	FOnlineSubsystemOculus& OculusSubsystem;

	/** All the friends for the player */
	TMap<ovrID, TSharedRef<FOnlineFriend>> PlayerFriends;

	/** Invitable users to a room for the player */
	TMap<ovrID, TSharedRef<FOnlineFriend>> InvitableUsers;

PACKAGE_SCOPE:

	/**
	* ** INTERNAL **
	* Called when we get the results back from the MessageQueue
	*/
	void OnQueryFriendsComplete(ovrMessageHandle Message, bool bIsError, int32 LocalUserNum, const FString& ListName, TMap<ovrID, TSharedRef<FOnlineFriend>>& OutList, bool bAppendToExistingMap, const FOnReadFriendsListComplete& Delegate);

public:

	static const FString FriendsListInviteableUsers;

	/**
	* Constructor
	*
	* @param InSubsystem - A reference to the owning subsystem
	*/
	FOnlineFriendsOculus(FOnlineSubsystemOculus& InSubsystem);

	/**
	* Default destructor
	*/
	virtual ~FOnlineFriendsOculus() = default;

	// Begin IOnlineFriends interface
	virtual bool ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate = FOnReadFriendsListComplete()) override;
	virtual bool DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate = FOnDeleteFriendsListComplete()) override;
	virtual bool SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate = FOnSendInviteComplete()) override;
	virtual bool AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate = FOnAcceptInviteComplete()) override;
	virtual bool RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends) override;
	virtual TSharedPtr<FOnlineFriend> GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace) override;
	virtual bool GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers) override;
	virtual bool BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool QueryBlockedPlayers(const FUniqueNetId& UserId) override;
	virtual bool GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers) override;
	virtual void DumpBlockedPlayers() const override;
	// End IOnlineFriends interface
};
