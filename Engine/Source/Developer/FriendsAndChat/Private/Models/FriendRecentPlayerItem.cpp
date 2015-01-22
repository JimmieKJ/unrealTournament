// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "FriendRecentPlayerItems.h"


// FFriendStruct implementation

const TSharedPtr< FOnlineFriend > FFriendRecentPlayerItem::GetOnlineFriend() const
{
	return nullptr;
}

const TSharedPtr< FOnlineUser > FFriendRecentPlayerItem::GetOnlineUser() const
{
	return nullptr;
}

const FString FFriendRecentPlayerItem::GetName() const
{
	if(OnlineUser.IsValid())
	{
		return OnlineUser->GetDisplayName();
	}
	return RecentPlayer->GetDisplayName();
}

const FText FFriendRecentPlayerItem::GetFriendLocation() const
{
	return FText::GetEmpty();
}

const FString FFriendRecentPlayerItem::GetClientId() const
{
	return TEXT("");
}

const bool FFriendRecentPlayerItem::IsOnline() const
{
	return false;
}

EOnlinePresenceState::Type FFriendRecentPlayerItem::GetOnlineStatus() const
{
	return EOnlinePresenceState::Offline;
}

const TSharedRef< FUniqueNetId > FFriendRecentPlayerItem::GetUniqueID() const
{
	return RecentPlayer->GetUserId();
}

const EFriendsDisplayLists::Type FFriendRecentPlayerItem::GetListType() const
{
	return EFriendsDisplayLists::RecentPlayersDisplay;
}

void FFriendRecentPlayerItem::SetOnlineFriend( TSharedPtr< FOnlineFriend > InOnlineFriend )
{
}

void FFriendRecentPlayerItem::SetOnlineUser(TSharedPtr< FOnlineUser > InOnlineUser)
{
	OnlineUser = InOnlineUser;
}

void FFriendRecentPlayerItem::ClearUpdated()
{
}

bool FFriendRecentPlayerItem::IsUpdated()
{
	return false;
}

void FFriendRecentPlayerItem::SetPendingAccept()
{
}

bool FFriendRecentPlayerItem::IsPendingAccepted() const
{
	return false;
}

bool FFriendRecentPlayerItem::IsGameRequest() const
{
	return false;
}

bool FFriendRecentPlayerItem::IsGameJoinable() const
{
	return false;
}

FString FFriendRecentPlayerItem::GetGameSessionId() const
{
	return FString();
}

void FFriendRecentPlayerItem::SetPendingInvite()
{
}

void FFriendRecentPlayerItem::SetPendingDelete()
{
}

bool FFriendRecentPlayerItem::IsPendingDelete() const
{
	return false;
}

EInviteStatus::Type FFriendRecentPlayerItem::GetInviteStatus()
{
	return EInviteStatus::Unknown;
}

#undef LOCTEXT_NAMESPACE
