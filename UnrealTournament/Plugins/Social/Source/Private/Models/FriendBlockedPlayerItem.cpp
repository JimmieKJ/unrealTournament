// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendBlockedPlayerItem.h"


// FFriendStruct implementation

const TSharedPtr< FOnlineFriend > FFriendBlockedPlayerItem::GetOnlineFriend() const
{
	return nullptr;
}

const TSharedPtr< FOnlineUser > FFriendBlockedPlayerItem::GetOnlineUser() const
{
	return BlockedPlayer;
}

const FString FFriendBlockedPlayerItem::GetName() const
{
	if(OnlineUser.IsValid())
	{
		return OnlineUser->GetDisplayName();
	}
	if (BlockedPlayer.IsValid())
	{
		return BlockedPlayer->GetDisplayName();
	}
	return Username.ToString();
}

const FText FFriendBlockedPlayerItem::GetFriendLocation() const
{
	return FText::GetEmpty();
}

const FString FFriendBlockedPlayerItem::GetAppId() const
{
	return TEXT("");
}

FDateTime FFriendBlockedPlayerItem::GetLastSeen() const
{
	return FDateTime::UtcNow();
}

const bool FFriendBlockedPlayerItem::IsOnline() const
{
	return false;
}

const EOnlinePresenceState::Type FFriendBlockedPlayerItem::GetOnlineStatus() const
{
	return EOnlinePresenceState::Offline;
}

const TSharedRef<const FUniqueNetId> FFriendBlockedPlayerItem::GetUniqueID() const
{
	if (BlockedPlayer.IsValid())
	{
		return BlockedPlayer->GetUserId();
	}
	return UniqueID.ToSharedRef();
}

const EFriendsDisplayLists::Type FFriendBlockedPlayerItem::GetListType() const
{
	return EFriendsDisplayLists::BlockedPlayersDisplay;
}

void FFriendBlockedPlayerItem::SetOnlineFriend( TSharedPtr< FOnlineFriend > InOnlineFriend )
{
}

void FFriendBlockedPlayerItem::SetOnlineUser(TSharedPtr< FOnlineUser > InOnlineUser)
{
	OnlineUser = InOnlineUser;
}

void FFriendBlockedPlayerItem::ClearUpdated()
{
}

bool FFriendBlockedPlayerItem::IsUpdated()
{
	return false;
}

void FFriendBlockedPlayerItem::SetPendingAccept()
{
}

bool FFriendBlockedPlayerItem::IsPendingAccepted() const
{
	return false;
}

bool FFriendBlockedPlayerItem::IsGameRequest() const
{
	return false;
}

bool FFriendBlockedPlayerItem::IsGameJoinable() const
{
	return false;
}

bool FFriendBlockedPlayerItem::CanInvite() const
{
	return false;
}

bool FFriendBlockedPlayerItem::IsInParty() const
{
	return false;
}

bool FFriendBlockedPlayerItem::CanJoinParty() const
{
	return false;
}

TSharedPtr<const FUniqueNetId> FFriendBlockedPlayerItem::GetGameSessionId() const
{
	return nullptr;
}

TSharedPtr<IOnlinePartyJoinInfo> FFriendBlockedPlayerItem::GetPartyJoinInfo() const
{
	return nullptr;
}

void FFriendBlockedPlayerItem::SetPendingInvite()
{
}

void FFriendBlockedPlayerItem::SetPendingDelete()
{
}

bool FFriendBlockedPlayerItem::IsPendingDelete() const
{
	return false;
}

EInviteStatus::Type FFriendBlockedPlayerItem::GetInviteStatus()
{
	return EInviteStatus::Blocked;
}

void FFriendBlockedPlayerItem::SetWebImageService(const TSharedRef<class FWebImageService>& WebImageService)
{

}

const FSlateBrush* FFriendBlockedPlayerItem::GetPresenceBrush() const
{
	return nullptr;
}