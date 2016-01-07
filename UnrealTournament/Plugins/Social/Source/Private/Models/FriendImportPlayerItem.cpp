// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendImportPlayerItem.h"


// FFriendStruct implementation

const TSharedPtr< FOnlineFriend > FFriendImportPlayerItem::GetOnlineFriend() const
{
	return nullptr;
}

const TSharedPtr< FOnlineUser > FFriendImportPlayerItem::GetOnlineUser() const
{
	return OnlineUser;
}

const FString FFriendImportPlayerItem::GetName() const
{
	FString ExternalDisplayName;
	FString AuthTypeLower(AuthType.ToLower());
	if (OnlineUser->GetUserAttribute(FString::Printf(TEXT("%s:displayname"), *AuthTypeLower), ExternalDisplayName))
	{
		return ExternalDisplayName;
	}
	return OnlineUser->GetDisplayName();
}

const FText FFriendImportPlayerItem::GetFriendLocation() const
{
	return FText::GetEmpty();
}

const FString FFriendImportPlayerItem::GetAppId() const
{
	return TEXT("");
}

FDateTime FFriendImportPlayerItem::GetLastSeen() const
{
	return FDateTime::UtcNow();
}

const bool FFriendImportPlayerItem::IsOnline() const
{
	return false;
}

const EOnlinePresenceState::Type FFriendImportPlayerItem::GetOnlineStatus() const
{
	return EOnlinePresenceState::Offline;
}

const TSharedRef<const FUniqueNetId> FFriendImportPlayerItem::GetUniqueID() const
{
	return OnlineUser->GetUserId();
}

const EFriendsDisplayLists::Type FFriendImportPlayerItem::GetListType() const
{
	return EFriendsDisplayLists::SuggestedFriendsDisplay;
}

void FFriendImportPlayerItem::SetOnlineFriend( TSharedPtr< FOnlineFriend > InOnlineFriend )
{
}

void FFriendImportPlayerItem::SetOnlineUser(TSharedPtr< FOnlineUser > InOnlineUser)
{
}

void FFriendImportPlayerItem::ClearUpdated()
{
}

bool FFriendImportPlayerItem::IsUpdated()
{
	return false;
}

void FFriendImportPlayerItem::SetPendingAccept()
{
}

bool FFriendImportPlayerItem::IsPendingAccepted() const
{
	return false;
}

bool FFriendImportPlayerItem::IsGameRequest() const
{
	return false;
}

bool FFriendImportPlayerItem::IsGameJoinable() const
{
	return false;
}

bool FFriendImportPlayerItem::CanInvite() const
{
	return false;
}

bool FFriendImportPlayerItem::IsInParty() const
{
	return false;
}

bool FFriendImportPlayerItem::CanJoinParty() const
{
	return false;
}

TSharedPtr<const FUniqueNetId> FFriendImportPlayerItem::GetGameSessionId() const
{
	return nullptr;
}

TSharedPtr<IOnlinePartyJoinInfo> FFriendImportPlayerItem::GetPartyJoinInfo() const
{
	return nullptr;
}

void FFriendImportPlayerItem::SetPendingInvite()
{
}

void FFriendImportPlayerItem::SetPendingDelete()
{
}

bool FFriendImportPlayerItem::IsPendingDelete() const
{
	return false;
}

EInviteStatus::Type FFriendImportPlayerItem::GetInviteStatus()
{
	return EInviteStatus::Unknown;
}

void FFriendImportPlayerItem::SetWebImageService(const TSharedRef<class FWebImageService>& WebImageService)
{

}

const FSlateBrush* FFriendImportPlayerItem::GetPresenceBrush() const
{
	return nullptr;
}