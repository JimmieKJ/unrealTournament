// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFriendItem.h"
/**
 * Class containing the friend information - used to build the list view.
 */
class FFriendFriendItem : public FFriendItem
{
public:

	// FFriendStuct overrides

	virtual bool IsGameJoinable() const override;
	virtual bool CanJoinParty() const override;
	virtual bool CanInvite() const override;
	virtual TSharedPtr<IOnlinePartyJoinInfo> GetPartyJoinInfo() const override;

	// FFriendFriendItem

	FFriendFriendItem(TSharedPtr< FOnlineFriend > InOnlineFriend, TSharedPtr< FOnlineUser > InOnlineUser, EFriendsDisplayLists::Type InListType, const TSharedRef<class FFriendsService>& InFriendsService)
		: FFriendItem(InOnlineFriend, InOnlineUser, InListType)
		, FriendsService(InFriendsService)
	{ }

private:

	TSharedPtr<class FFriendsService> FriendsService;
};
