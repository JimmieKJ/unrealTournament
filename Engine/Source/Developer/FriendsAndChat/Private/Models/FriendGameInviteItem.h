// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IFriendItem.h"
/**
 * Class containing a friend's game invite information - used to build the list view.
 */
class FFriendGameInviteItem : 
	public FFriendItem
{
public:

	// FFriendStuct overrides

	virtual bool IsGameRequest() const override;
	virtual bool IsGameJoinable() const override;
	virtual FString GetGameSessionId() const override;

	// FFriendGameInviteItem

	FFriendGameInviteItem(
		const TSharedRef<FOnlineUser>& InOnlineUser,
		const TSharedRef<FOnlineSessionSearchResult>& InSessionResult)
		: FFriendItem(nullptr, InOnlineUser, EFriendsDisplayLists::GameInviteDisplay)
		, SessionResult(InSessionResult)
	{ }

protected:

	/** Hidden default constructor. */
	FFriendGameInviteItem()
	{ }

private:

	TSharedPtr<FOnlineSessionSearchResult> SessionResult;
};
