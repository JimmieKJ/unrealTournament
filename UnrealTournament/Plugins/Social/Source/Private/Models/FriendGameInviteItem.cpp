// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendGameInviteItem.h"

#include "GameAndPartyService.h"

// FFriendGameInviteItem

bool FFriendGameInviteItem::CanJoinParty() const
{
	TSharedPtr<IOnlinePartyJoinInfo> PartyInfo = GetPartyJoinInfo();
	return PartyInfo.IsValid() && PartyInfo->CanJoin() && !GameAndPartyService->IsFriendInSameParty(AsShared());
}

bool FFriendGameInviteItem::CanInvite() const
{
	FString FriendsAppId = GetAppId();
	return FriendsAppId == GameAndPartyService->GetUserAppId() || FFriendItem::LauncherAppIds.Contains(FriendsAppId);
}

TSharedPtr<IOnlinePartyJoinInfo> FFriendGameInviteItem::GetPartyJoinInfo() const
{
	// obtain party info from presence
	return GameAndPartyService->GetPartyJoinInfo(AsShared());
}

bool FFriendGameInviteItem::IsGameRequest() const
{
	return true;
}

bool FFriendGameInviteItem::IsGameJoinable() const
{
	const TSharedPtr<const IFriendItem> Item = AsShared();
	return !GameAndPartyService->IsFriendInSameSession(Item);
}

TSharedPtr<const FUniqueNetId> FFriendGameInviteItem::GetGameSessionId() const
{
	TSharedPtr<const FUniqueNetId> SessionId = NULL;
	if (SessionResult->Session.SessionInfo.IsValid())
	{
		SessionId = GameAndPartyService->GetGameSessionId(SessionResult->Session.SessionInfo->GetSessionId().ToString());
	}
	return SessionId;
}

const FString FFriendGameInviteItem::GetAppId() const
{
	return AppId;
}

const EOnlinePresenceState::Type FFriendGameInviteItem::GetOnlineStatus() const
{
	return EOnlinePresenceState::Online;
}
