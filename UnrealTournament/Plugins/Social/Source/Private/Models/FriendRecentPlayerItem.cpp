// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
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
	if (RecentPlayer.IsValid())
	{
		return RecentPlayer->GetDisplayName();
	}
	return Username.ToString();
}

const FText FFriendRecentPlayerItem::GetFriendLocation() const
{
	return FText::GetEmpty();
}

const FString FFriendRecentPlayerItem::GetAppId() const
{
	return TEXT("");
}

const FSlateBrush* FFriendRecentPlayerItem::GetPresenceBrush() const
{
	return nullptr;
}

FDateTime FFriendRecentPlayerItem::GetLastSeen() const
{
	if (RecentPlayer.IsValid())
	{
		return RecentPlayer->GetLastSeen();
	}
	return FDateTime::UtcNow();
}

const bool FFriendRecentPlayerItem::IsOnline() const
{
	return false;
}

const EOnlinePresenceState::Type FFriendRecentPlayerItem::GetOnlineStatus() const
{
	return EOnlinePresenceState::Offline;
}

const TSharedRef<const FUniqueNetId> FFriendRecentPlayerItem::GetUniqueID() const
{
	if (RecentPlayer.IsValid())
	{
		return RecentPlayer->GetUserId();
	}
	return UniqueID.ToSharedRef();
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

bool FFriendRecentPlayerItem::CanInvite() const
{
	return false;
}

bool FFriendRecentPlayerItem::IsInParty() const
{
	return false;
}

bool FFriendRecentPlayerItem::CanJoinParty() const
{
	return false;
}

TSharedPtr<const FUniqueNetId> FFriendRecentPlayerItem::GetGameSessionId() const
{
	return nullptr;
}

TSharedPtr<IOnlinePartyJoinInfo> FFriendRecentPlayerItem::GetPartyJoinInfo() const
{
	return nullptr;
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

void FFriendRecentPlayerItem::SetWebImageService(const TSharedRef<class FWebImageService>& WebImageService)
{

}