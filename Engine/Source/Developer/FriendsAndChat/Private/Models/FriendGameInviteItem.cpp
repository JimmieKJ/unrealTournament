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
	FString SessionId = GetGameSessionId();
	if (!SessionId.IsEmpty())
	{
		if (SessionId != FFriendsAndChatManager::Get()->GetGameSessionId())
		{
			return true;
		}
	}
	return false;
}

FString FFriendGameInviteItem::GetGameSessionId() const
{
	FString SessionId;
	if (SessionResult->Session.SessionInfo.IsValid())
	{
		SessionId = SessionResult->Session.SessionInfo->GetSessionId().ToString();
	}
	return SessionId;
}
