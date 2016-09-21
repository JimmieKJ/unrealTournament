// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineFriendsInterface.h"
#include "OnlineSharingFacebook.h"
#include "OnlineSubsystemFacebookPackage.h"
#include "OnlinePresenceInterface.h"

/**
 * Info associated with an online friend on the facebook service
 */
class FOnlineFriendFacebook : 
	public FOnlineFriend
{
public:

	// FOnlineUser

	virtual TSharedRef<const FUniqueNetId> GetUserId() const override;
	virtual FString GetRealName() const override;
	virtual FString GetDisplayName(const FString& Platform = FString()) const override;
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;

	// FOnlineFriend
	
	virtual EInviteStatus::Type GetInviteStatus() const override;
	virtual const FOnlineUserPresence& GetPresence() const override;

	// FOnlineFriendMcp

	/**
	 * Init/default constructor
	 */
	FOnlineFriendFacebook(const FString& InUserId=TEXT("")) 
		: UserId(new FUniqueNetIdString(InUserId))
	{
	}

	/**
	 * Destructor
	 */
	virtual ~FOnlineFriendFacebook()
	{
	}

	/**
	 * Get account data attribute
	 *
	 * @param Key account data entry key
	 * @param OutVal [out] value that was found
	 *
	 * @return true if entry was found
	 */
	inline bool GetAccountData(const FString& Key, FString& OutVal) const
	{
		const FString* FoundVal = AccountData.Find(Key);
		if (FoundVal != NULL)
		{
			OutVal = *FoundVal;
			return true;
		}
		return false;
	}

	/** User Id represented as a FUniqueNetId */
	TSharedRef<const FUniqueNetId> UserId;
	/** Any addition account data associated with the friend */
	TMap<FString, FString> AccountData;
	/** @temp presence info */
	FOnlineUserPresence Presence;
};


/**
 * Facebook service implementation of the online friends interface
 */
class FOnlineFriendsFacebook :
	public IOnlineFriends
{

public:

	// IOnlineFriends

	virtual bool ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate = FOnReadFriendsListComplete()) override;
	virtual bool DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate = FOnDeleteFriendsListComplete()) override;
	virtual bool SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName,  const FOnSendInviteComplete& Delegate = FOnSendInviteComplete()) override;
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

	// FOnlineFriendsFacebook

PACKAGE_SCOPE:

	/**
	 * Constructor
	 *
	 * @param InSubsystem Facebook subsystem being used
	 */
	FOnlineFriendsFacebook(class FOnlineSubsystemFacebook* InSubsystem);


public:
	
	/**
	 * Destructor
	 */
	virtual ~FOnlineFriendsFacebook();


private:

	/**
	 * Should use the OSS param constructor instead
	 */
	FOnlineFriendsFacebook();


private:

	/**
	 * Delegate fired when we have a response after we have requested read permissions of the users friends list
	 *
	 * @param LocalUserNum		- The local player number the friends were read for
	 * @param bWasSuccessful	- Whether the permissions were updated successfully
	 */
	void OnReadFriendsPermissionsUpdated(int32 LocalUserNum, bool bWasSuccessful);

	/**
	 * Using the open graph for fb to read friends list, called when we have ensured correct permissions
	 *
	 * @param LocalUserNum - The local player number the friends were read for
	 * @param ListName - named list to read
	 */
	void ReadFriendsUsingGraphPath(int32 LocalUserNum, const FString& ListName);

	/** Handle to delegate used to notify this interface that permissions are updated, and that we can now read the friends list */
	FDelegateHandle RequestFriendsReadPermissionsDelegateHandle;

	/** Delegate called when reading the friends list is completed */
	FOnReadFriendsListComplete OnReadFriendsListCompleteDelegate;

private:

	/** Reference to the main facebook identity */
	IOnlineIdentityPtr IdentityInterface;

	/** Reference to the main facebook sharing interface */
	IOnlineSharingPtr SharingInterface;

	/** The collection of facebook friends received through the fb callbacks in ReadFriendsList */
	TArray< TSharedRef<FOnlineFriendFacebook> > CachedFriends;

	/** Config based list of fields to use when querying friends list */
	TArray<FString> FriendsFields;
};

typedef TSharedPtr<FOnlineFriendsFacebook, ESPMode::ThreadSafe> FOnlineFriendsFacebookPtr;
