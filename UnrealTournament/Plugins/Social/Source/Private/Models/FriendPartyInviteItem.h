// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFriendItem.h"
/**
 * Class containing a friend's party invite information - used to build the list view.
 */
class FFriendPartyInviteItem : 
	public FFriendItem
{
public:

	// FFriendStuct overrides

	virtual bool CanJoinParty() const override;
	virtual bool CanInvite() const override;
	virtual bool IsGameRequest() const override;
	virtual bool IsGameJoinable() const override;
	virtual TSharedPtr<const FUniqueNetId> GetGameSessionId() const override;
	virtual TSharedPtr<IOnlinePartyJoinInfo> GetPartyJoinInfo() const override;
	virtual const FString GetAppId() const override;
	virtual const FString GetName() const override;
	virtual const EOnlinePresenceState::Type GetOnlineStatus() const override;

	// FFriendPartyInviteItem

	FFriendPartyInviteItem(
		const TSharedRef<FOnlineUser>& InOnlineUser,
		const FString& InAppId,
		const TSharedRef<IOnlinePartyJoinInfo>& InPartyJoinInfo,
		const TSharedRef<class FGameAndPartyService>& InGameAndPartyService)
		: FFriendItem(nullptr, InOnlineUser, EFriendsDisplayLists::GameInviteDisplay)
		, PartyJoinInfo(InPartyJoinInfo)
		, AppId(InAppId)
		, GameAndPartyService(InGameAndPartyService)
	{ }

protected:

	/** Hidden default constructor. */
	FFriendPartyInviteItem()
	{ }

private:

	TSharedPtr<IOnlinePartyJoinInfo> PartyJoinInfo;
	FString AppId;
	TSharedPtr<class FGameAndPartyService> GameAndPartyService;
};
