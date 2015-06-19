// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendGameInviteItem.h"

// FFriendGameInviteItem

bool FFriendGameInviteItem::IsGameRequest() const
{
	return true;
}

bool FFriendGameInviteItem::IsGameJoinable() const
{
	const TSharedPtr<const IFriendItem> Item = AsShared();
	return !FFriendsAndChatManager::Get()->IsFriendInSameSession(Item);
}

TSharedPtr<FUniqueNetId> FFriendGameInviteItem::GetGameSessionId() const
{
	TSharedPtr<FUniqueNetId> SessionId = NULL;
	if (SessionResult->Session.SessionInfo.IsValid())
	{
		SessionId = MakeShareable(new FUniqueNetIdString(SessionResult->Session.SessionInfo->GetSessionId().ToString()));
	}
	return SessionId;
}

const FString FFriendGameInviteItem::GetClientId() const
{
	return ClientId;
}