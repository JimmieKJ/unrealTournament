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

	virtual bool CanJoinParty() const override;
	virtual bool CanInvite() const override;
	virtual TSharedPtr<IOnlinePartyJoinInfo> GetPartyJoinInfo() const override;
	virtual bool IsGameRequest() const override;
	virtual bool IsGameJoinable() const override;
	virtual TSharedPtr<const FUniqueNetId> GetGameSessionId() const override;
	virtual const FString GetAppId() const override;
	virtual const EOnlinePresenceState::Type GetOnlineStatus() const override;
	// FFriendGameInviteItem

	FFriendGameInviteItem(
		const TSharedRef<FOnlineUser>& InOnlineUser,
		const TSharedRef<FOnlineSessionSearchResult>& InSessionResult,
		const FString& InAppId,
		const TSharedRef<FOnlineFriend>& InOnlineFriend,
		const TSharedRef<class FGameAndPartyService>& InGameAndPartyService)
		: FFriendItem(InOnlineFriend, InOnlineUser, EFriendsDisplayLists::GameInviteDisplay)
		, SessionResult(InSessionResult)
		, AppId(InAppId)
		, GameAndPartyService(InGameAndPartyService)
	{ }

private:

	TSharedPtr<FOnlineSessionSearchResult> SessionResult;
	FString AppId;
	TSharedPtr<class FGameAndPartyService> GameAndPartyService;
};
