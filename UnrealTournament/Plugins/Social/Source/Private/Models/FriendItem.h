// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFriendItem.h"
/**
 * Class containing the friend information - used to build the list view.
 */
class FFriendItem : public IFriendItem 
{
public:

	const static FString LauncherAppIds;
	const static FString FortniteAppIds;
	const static FString FortniteLiveAppIds;
	const static FString FortnitePublicTestAppIds;
	const static FString UnrealTournamentAppIds;

	/**
	 * Constructor takes the required details.
	 *
	 * @param InOnlineFriend The online friend.
	 * @param InOnlineUser The online user.
	 * @param InListType The list type.
	 */
	FFriendItem(TSharedPtr< FOnlineFriend > InOnlineFriend, TSharedPtr< FOnlineUser > InOnlineUser, EFriendsDisplayLists::Type InListType)
		: bIsUpdated(true)
		, GroupName(TEXT(""))
		, OnlineFriend( InOnlineFriend )
		, OnlineUser( InOnlineUser )
		, UniqueID( InOnlineUser->GetUserId() )
		, ListType( InListType )
		, bIsPendingAccepted(false)
		, bIsPendingInvite(false)
		, bIsPendingDelete(false)
	{ }

	/**
	 * Constructor takes the required details.
	 *
	 * @param InGroupName The group name.
	 */
	FFriendItem( const FString& InGroupName )
		: GroupName( InGroupName )
	{ }

	/** 
	 * Get the display name of a player based on the platform they are using
	 */
	static FString GetNameByActivePlatform(TSharedPtr< FOnlineFriend > InOnlineFriend, TSharedPtr< FOnlineUser > InOnlineUser);


public:

	/**
	* Gets if the friend is playing on a PC or console.
	*
	* @return true if playing on PC or Mac.
	*/
	virtual bool IsPCPlatform() const override;

	/**
	 * Get the on-line user associated with this account.
	 *
	 * @return The online user.
	 * @see GetOnlineFriend
	 */
	virtual const TSharedPtr< FOnlineUser > GetOnlineUser() const override;

	/**
	 * Get the cached on-line Friend.
	 *
	 * @return The online friend.
	 * @see GetOnlineUser, SetOnlineFriend
	 */
	virtual const TSharedPtr< FOnlineFriend > GetOnlineFriend() const override;

	/**
	 * Get the cached user name.
	 * @return The user name.
	 */
	virtual const FString GetName() const override;
	
	/**
	 * Get the user location.
	 * @return The user location.
	 */
	virtual const FText GetFriendLocation() const override;

	/**
	 * Get the client the user is logged in on
	 * @return The client id of the user
	 */
	virtual const FString GetAppId() const override;

	virtual const FSlateBrush* GetPresenceBrush() const override;

	/**
	* Get last seen time
	* @return The time
	*/
	virtual FDateTime GetLastSeen() const override;

	/**
	 * Get if the user is online.
	 * @return The user online state.
	 */
	virtual const bool IsOnline() const override;

	/**
	 * Get the online status of the user
	 * @return online presence status
	 */
	virtual const EOnlinePresenceState::Type GetOnlineStatus() const override;

	/**
	 * Get if the user is online and his game is joinable
	 * @return The user joinable game state.
	 */
	virtual bool IsGameJoinable() const override;

	/**
	 * Get if the user is in a party
	 * @return The user in party state.
	 */
	virtual bool IsInParty() const override;

	/**
	 * Get if the user is in a joinable party
	 * @return The user joinable party state.
	 */
	virtual bool CanJoinParty() const override;

	/**
	* Get if the user can join our game if we were to invite them
	* @return True if we can invite them
	*/
	virtual bool CanInvite() const override;

	/**
	 * Get if the user is online and his game is joinable
	 * @return The user joinable game state.
	 */
	virtual TSharedPtr<const FUniqueNetId> GetGameSessionId() const override;

	/**
	 * Obtain info needed to join a party for this friend item
	 * @return party info if available or null
	 */
	virtual TSharedPtr<IOnlinePartyJoinInfo> GetPartyJoinInfo() const override;

	/**
	 * Get the Unique ID.
	 * @return The Unique Net ID.
	 */
	virtual const TSharedRef<const FUniqueNetId> GetUniqueID() const override;

	/**
	 * Is this friend in the default list.
	 * @return The List Type.
	 */
	virtual const EFriendsDisplayLists::Type GetListType() const override;

	/**
	 * Set new online friend.
	 *
	 * @param InOnlineFriend The new online friend.
	 * @see GetOnlineFriend
	 */
	virtual void SetOnlineFriend( TSharedPtr< FOnlineFriend > InOnlineFriend ) override;

	/**
	 * Set new online user.
	 *
	 * @param InOnlineUser The new online user.
	 */
	virtual void SetOnlineUser( TSharedPtr< FOnlineUser > InOnlineUser) override;

	/**
	 * Clear updated flag.
	 */
	virtual void ClearUpdated() override;

	/**
	 * Check if we have been updated.
	 *
	 * @return true if updated.
	 */
	virtual bool IsUpdated() override;

	/** Set if pending invitation response. */
	virtual void SetPendingInvite() override;

	/** Set if pending invitation response. */
	virtual void SetPendingAccept() override;

	/** Set if pending delete. */
	virtual void SetPendingDelete() override;

	/** Get if pending delete. */
	virtual bool IsPendingDelete() const override;

	/** Get if pending invitation response. */
	virtual bool IsPendingAccepted() const override;

	/** Get if is from a game request. */
	virtual bool IsGameRequest() const override;

	/**
	 * Get the invitation status.
	 *
	 * @return The invitation status.
	 */
	virtual EInviteStatus::Type GetInviteStatus() override;

	virtual void SetWebImageService(const TSharedRef<class FWebImageService>& WebImageService) override;

protected:

	/** Hidden default constructor. */
	FFriendItem()
		: bIsUpdated(true)
		, GroupName(TEXT(""))
	{ };

private:

	/** Holds if this item has been updated. */
	bool bIsUpdated;

	/** Holds the group name. */
	const FString GroupName;

	/** Holds the cached online friend. */
	TSharedPtr<FOnlineFriend> OnlineFriend;

	/** Holds the cached online user. */
	TSharedPtr<FOnlineUser> OnlineUser;

	/** Holds the cached user id. */
	TSharedPtr<const FUniqueNetId> UniqueID;

	/** Holds if this is the list type. */
	EFriendsDisplayLists::Type ListType;

	/** Holds if we are pending an accept as friend action. */
	bool bIsPendingAccepted;

	/** Holds if we are pending an invite response. */
	bool bIsPendingInvite;

	/** Holds if we are pending delete. */
	bool bIsPendingDelete;

	TSharedPtr<FWebImageService> WebImageService;
};
