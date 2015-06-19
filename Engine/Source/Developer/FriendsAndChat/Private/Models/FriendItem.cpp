// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"


#define LOCTEXT_NAMESPACE "FriendListItems"


// FFriendStruct implementation

const FString FFriendItem::LauncherClientIds("f3e80378aed4462498774a7951cd263f, 34a02cf8f4414e29b15921876da36f9a");
const FString FFriendItem::FortniteClientId("300d79839c914445948e3c1100f211db");
const FString FFriendItem::UnrealTournamentClientId("1252412dc7704a9690f6ea4611bc81ee");

const TSharedPtr< FOnlineFriend > FFriendItem::GetOnlineFriend() const
{
	return OnlineFriend;
}

const TSharedPtr< FOnlineUser > FFriendItem::GetOnlineUser() const
{
	return OnlineUser;
}

const FString FFriendItem::GetName() const
{
	if ( OnlineUser.IsValid() )
	{
		return OnlineUser->GetDisplayName();
	}
	
	return GroupName;
}

const FText FFriendItem::GetFriendLocation() const
{
	if(OnlineFriend.IsValid())
	{
		const FOnlineUserPresence& OnlinePresence = OnlineFriend->GetPresence();

		// TODO - Localize text once final format is decided
		if (OnlinePresence.bIsOnline)
		{
			switch (OnlinePresence.Status.State)
			{
			case EOnlinePresenceState::Offline:
				return FText::FromString("Offline");
			case EOnlinePresenceState::Away:
			case EOnlinePresenceState::ExtendedAway:
				return FText::FromString("Away");
			case EOnlinePresenceState::Chat:
			case EOnlinePresenceState::DoNotDisturb:
			case EOnlinePresenceState::Online:	
			default:
				// rich presence string
				if (!OnlinePresence.Status.StatusStr.IsEmpty())
				{
					return FText::FromString(*OnlinePresence.Status.StatusStr);
				}
				else
				{
					return FText::FromString(TEXT("Online"));
				}
			};
		}
		else
		{
			return FText::FromString("Offline");
		}
	}
	return FText::GetEmpty();
}

const FString FFriendItem::GetClientId() const
{
	FString Result;
	if (OnlineFriend.IsValid())
	{
		const FOnlineUserPresence& OnlinePresence = OnlineFriend->GetPresence();
		const FVariantData* ClientId = OnlinePresence.Status.Properties.Find(DefaultClientIdKey);
		if (ClientId != nullptr &&
			ClientId->GetType() == EOnlineKeyValuePairDataType::String)
		{
			ClientId->GetValue(Result);
		}
	}
	return Result;
}

const FString FFriendItem::GetClientName() const
{
	FString Result;
	FString ClientId = GetClientId();

	if (!ClientId.IsEmpty())
	{
		// hardcoded for now, need a generic way to receive client names or map client ids to names
		if (ClientId == FFriendItem::FortniteClientId)
		{
			Result = TEXT("Fortnite");
		}
		else if (ClientId == FFriendItem::UnrealTournamentClientId)
		{
			Result = TEXT("Unreal Tournament");
		}
		else if (FFriendItem::LauncherClientIds.Contains(ClientId))
		{
			Result = TEXT("Epic Games Launcher");
		}
	}
	return Result;
}

const TSharedPtr<FUniqueNetId> FFriendItem::GetSessionId() const
{
	if (OnlineFriend.IsValid())
	{
		const FOnlineUserPresence& OnlinePresence = OnlineFriend->GetPresence();
		if (OnlinePresence.SessionId.IsValid())
		{
			return OnlinePresence.SessionId;
		}
	}
	return nullptr;
}

const bool FFriendItem::IsOnline() const
{
	if(OnlineFriend.IsValid())
	{
		return OnlineFriend->GetPresence().Status.State != EOnlinePresenceState::Offline ? true : false;
	}
	return false;
}

const EOnlinePresenceState::Type FFriendItem::GetOnlineStatus() const
{
	if (OnlineFriend.IsValid())
	{
		return OnlineFriend->GetPresence().Status.State;
	}
	return EOnlinePresenceState::Offline;
}

bool FFriendItem::IsGameJoinable() const
{
	if (OnlineFriend.IsValid())
	{
		const FOnlineUserPresence& FriendPresence = OnlineFriend->GetPresence();
		const bool bIsOnline = FriendPresence.Status.State != EOnlinePresenceState::Offline;
		bool bIsJoinable = FriendPresence.bIsJoinable && !FFriendsAndChatManager::Get()->IsFriendInSameSession(AsShared());

		return bIsOnline && bIsJoinable;
	}
	return false;
}

bool FFriendItem::CanInvite() const
{
	FString FriendsClientID = GetClientId();
	return FriendsClientID == FFriendsAndChatManager::Get()->GetUserClientId() || FFriendItem::LauncherClientIds.Contains(FriendsClientID);
}

TSharedPtr<FUniqueNetId> FFriendItem::GetGameSessionId() const
{
	if (OnlineFriend.IsValid())
	{
		const FOnlineUserPresence& FriendPresence = OnlineFriend->GetPresence();
		if (FriendPresence.SessionId.IsValid())
		{
			return FriendPresence.SessionId;
		}
	}
	return nullptr;
}

const TSharedRef< FUniqueNetId > FFriendItem::GetUniqueID() const
{
	return UniqueID.ToSharedRef();
}

const EFriendsDisplayLists::Type FFriendItem::GetListType() const
{
	return ListType;
}

void FFriendItem::SetOnlineFriend( TSharedPtr< FOnlineFriend > InOnlineFriend )
{
	OnlineFriend = InOnlineFriend;
	bIsUpdated = true;
}

void FFriendItem::SetOnlineUser(TSharedPtr< FOnlineUser > InOnlineUser)
{
	OnlineUser = InOnlineUser;
}

void FFriendItem::ClearUpdated()
{
	bIsUpdated = false;
	bIsPendingAccepted = false;
	bIsPendingInvite = false;
	bIsPendingDelete = false;
}

bool FFriendItem::IsUpdated()
{
	return bIsUpdated;
}

void FFriendItem::SetPendingAccept()
{
	bIsPendingAccepted = true;
}

bool FFriendItem::IsPendingAccepted() const
{
	return bIsPendingAccepted;
}

bool FFriendItem::IsGameRequest() const
{
	return false;
}

void FFriendItem::SetPendingInvite()
{
	bIsPendingInvite = true;
}

void FFriendItem::SetPendingDelete()
{
	bIsPendingDelete = true;
}

bool FFriendItem::IsPendingDelete() const
{
	return bIsPendingDelete;
}

EInviteStatus::Type FFriendItem::GetInviteStatus()
{
	if ( bIsPendingAccepted )
	{
		return EInviteStatus::Accepted;
	}
	else if ( bIsPendingInvite )
	{
		return EInviteStatus::PendingOutbound;
	}
	else if ( OnlineFriend.IsValid() )
	{
		return OnlineFriend->GetInviteStatus();
	}

	return EInviteStatus::Unknown;
}

#undef LOCTEXT_NAMESPACE
