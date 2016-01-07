// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFriendItem.h"
/**
 * Class containing the friend information - used to build the list view.
 */
class FFriendBlockedPlayerItem : public IFriendItem 
{
public:

	/**
	 * Constructor takes the required details.
	 *
	 * @param BlockedPlayer The Blocked friend.
	 */
	FFriendBlockedPlayerItem(TSharedPtr< FOnlineBlockedPlayer > InBlockedPlayer)
		: BlockedPlayer(InBlockedPlayer)
	{ }

	FFriendBlockedPlayerItem(const TSharedPtr<const FUniqueNetId> InUniqueID, const FText InUsername)
		:UniqueID(InUniqueID)
		,Username(InUsername)
	{}

public:

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
	* @return The date last seen
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
	 * Get the Unique ID.
	 * @return The Unique Net ID.
	 */
	virtual const TSharedRef< const FUniqueNetId > GetUniqueID() const override;

	/**
	 * Is this friend in the default list.
	 * @return The List Type.
	 */
	virtual const EFriendsDisplayLists::Type GetListType() const override;

	virtual void SetOnlineFriend( TSharedPtr< FOnlineFriend > InOnlineFriend ) override;

	virtual void SetOnlineUser( TSharedPtr< FOnlineUser > InOnlineFriend ) override;

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

	/** Is the player in a game that is joinable */
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

	/** Get if the user can join our game if we were to invite them*/
	virtual bool CanInvite() const override;

	/** Get if the user is online and his game is joinable */
	virtual TSharedPtr<const FUniqueNetId> GetGameSessionId() const override;
	
	virtual TSharedPtr<IOnlinePartyJoinInfo> GetPartyJoinInfo() const override;

	/**
	 * Get the invitation status.
	 *
	 * @return The invitation status.
	 */
	virtual EInviteStatus::Type GetInviteStatus() override;

	virtual void SetWebImageService(const TSharedRef<class FWebImageService>& WebImageService) override;

private:

	/** Hidden default constructor. */
	FFriendBlockedPlayerItem()
	{ };

private:

	// Holds the Blocked player struct
	TSharedPtr< FOnlineBlockedPlayer > BlockedPlayer;
	TSharedPtr<FOnlineUser> OnlineUser;
	const TSharedPtr<const FUniqueNetId> UniqueID;
	FText Username;
};
