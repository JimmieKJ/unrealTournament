// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendPartyInviteItem.h"

#include "GameAndPartyService.h"

// FFriendPartyInviteItem

bool FFriendPartyInviteItem::CanJoinParty() const
{
	TSharedPtr<IOnlinePartyJoinInfo> PartyInfo = GetPartyJoinInfo();
	return PartyInfo.IsValid() && PartyInfo->CanJoin() && !GameAndPartyService->IsFriendInSameParty(AsShared());
}

bool FFriendPartyInviteItem::CanInvite() const
{
	FString FriendsAppId = GetAppId();
	return FriendsAppId == GameAndPartyService->GetUserAppId() || FFriendItem::LauncherAppIds.Contains(FriendsAppId);
}

bool FFriendPartyInviteItem::IsGameRequest() const
{
	return true;
}

bool FFriendPartyInviteItem::IsGameJoinable() const
{
	//@todo samz1 - make sure party invite hasn't already been accepted/rejected

	if (PartyJoinInfo.IsValid())
	{
		const TSharedPtr<const IFriendItem> Item = AsShared();
		return !GameAndPartyService->IsFriendInSameParty(Item);
	}
	return false;
}

TSharedPtr<const FUniqueNetId> FFriendPartyInviteItem::GetGameSessionId() const
{
	return nullptr;
}

TSharedPtr<IOnlinePartyJoinInfo> FFriendPartyInviteItem::GetPartyJoinInfo() const
{
	return PartyJoinInfo;
}

const FString FFriendPartyInviteItem::GetAppId() const
{
	return AppId;
}

const FString FFriendPartyInviteItem::GetName() const
{
	if (PartyJoinInfo.IsValid())
	{
		FString SourceDisplayName = PartyJoinInfo->GetSourceDisplayName();
		if (!SourceDisplayName.IsEmpty())
		{
			return SourceDisplayName;
		}
	}
	return FFriendItem::GetName();
}

const EOnlinePresenceState::Type FFriendPartyInviteItem::GetOnlineStatus() const
{
	return EOnlinePresenceState::Online;
}
