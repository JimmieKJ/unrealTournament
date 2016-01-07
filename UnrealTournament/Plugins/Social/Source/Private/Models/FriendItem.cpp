// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"

#include "WebImageService.h"

#define LOCTEXT_NAMESPACE "FriendListItems"


// FFriendStruct implementation

const FString FFriendItem::LauncherAppIds("f3e80378aed4462498774a7951cd263f, 34a02cf8f4414e29b15921876da36f9a, launcherDev, launcherLive");
const FString FFriendItem::FortniteLiveAppIds("ec684b8c687f479fadea3cb2ad83f5c6, 300d79839c914445948e3c1100f211db, fortniteLive");
const FString FFriendItem::FortnitePublicTestAppIds("7cf6977d137640759a51ec8fc88ebf9e, fortnitePublicTest");
const FString FFriendItem::FortniteAppIds("300d79839c914445948e3c1100f211db, eb1ce404a0e5446f8e43964a24db7bdd, 299a8d0ee5c84d43a94ffe9e54cc92dc, 3aa8093ea5f14165a3b5f77ba7f6c66a, 81545b947bda44a6bacd844af10e0716, df94a22141b24b02acfffc811d4d6159, 5a1fb298e1824563ad28296cb3a4008e, 807553cb40704839adcfc505c29920be, 7cf6977d137640759a51ec8fc88ebf9e, ec684b8c687f479fadea3cb2ad83f5c6, e02dd6fea2bb4f029bf529992cff8351, 807553cb40704839adcfc505c29920be, 7cf6977d137640759a51ec8fc88ebf9e, ec684b8c687f479fadea3cb2ad83f5c6, e02dd6fea2bb4f029bf529992cff8351, fortniteLive, fortnitePublicTest, fortniteDev");
const FString FFriendItem::UnrealTournamentAppIds("1252412dc7704a9690f6ea4611bc81ee, unrealTournamentLive");

const TSharedPtr< FOnlineFriend > FFriendItem::GetOnlineFriend() const
{
	return OnlineFriend;
}

FString FFriendItem::GetNameByActivePlatform(TSharedPtr< FOnlineFriend > InOnlineFriend, TSharedPtr< FOnlineUser > InOnlineUser)
{
	if (InOnlineFriend.IsValid() && InOnlineUser.IsValid())
	{
		const FOnlineUserPresence& FriendPresence = InOnlineFriend->GetPresence();
		const FVariantData* PlatformValue = FriendPresence.Status.Properties.Find(DefaultPlatformKey);
		if (PlatformValue && PlatformValue->GetType() == EOnlineKeyValuePairDataType::String)
		{
			FString Platform;
			PlatformValue->GetValue(Platform);

			if (!Platform.IsEmpty())
			{
				FString PlatformName;
				if (InOnlineUser->GetUserAttribute(Platform + TEXT(":displayname"), PlatformName) &&
					!PlatformName.IsEmpty())
				{
					return PlatformName;
				}
			}
		}
	}
	return FString();
}

bool FFriendItem::IsPCPlatform() const
{
	if (GetOnlineFriend().IsValid())
	{
		const FOnlineUserPresence& FriendPresence = GetOnlineFriend()->GetPresence();
		const FVariantData* PlatformValue = FriendPresence.Status.Properties.Find(DefaultPlatformKey);
		if (PlatformValue && PlatformValue->GetType() == EOnlineKeyValuePairDataType::String)
		{
			FString Platform;
			PlatformValue->GetValue(Platform);

			if (!Platform.IsEmpty())
			{
				return false;
			}
		}
	}
	return true;
}

const TSharedPtr< FOnlineUser > FFriendItem::GetOnlineUser() const
{
	return OnlineUser;
}

const FString FFriendItem::GetName() const
{
	// Try to get display name based on platform they are logged in with
	FString PlatformName = GetNameByActivePlatform(OnlineFriend, OnlineUser);
	if (!PlatformName.IsEmpty())
	{
		return PlatformName;
	}

	if (OnlineUser.IsValid())
	{
		FString OnlineUserName = OnlineUser->GetDisplayName();
		if (!OnlineUserName.IsEmpty())
		{
			return OnlineUserName;
		}
	}

	// Any other available display names?
	if (OnlineUser.IsValid())
	{
		static FString Platforms[] = { TEXT("psn") };
		for (int32 PlatformIndex = 0; PlatformIndex < ARRAY_COUNT(Platforms); ++PlatformIndex)
		{
			if (OnlineUser->GetUserAttribute(Platforms[PlatformIndex] + TEXT(":displayname"), PlatformName) &&
				!PlatformName.IsEmpty())
			{
				return PlatformName;
			}
		}
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

const FString FFriendItem::GetAppId() const
{
	FString Result;
	if (OnlineFriend.IsValid())
	{
		const FOnlineUserPresence& OnlinePresence = OnlineFriend->GetPresence();
		const FVariantData* AppId = OnlinePresence.Status.Properties.Find(DefaultAppIdKey);
		const FVariantData* OverrideAppId = OnlinePresence.Status.Properties.Find(OverrideClientIdKey);
		const FVariantData* LegacyId = OnlinePresence.Status.Properties.Find(TEXT("ClientId"));
		if (OverrideAppId != nullptr && OverrideAppId->GetType() == EOnlineKeyValuePairDataType::String)
		{
			OverrideAppId->GetValue(Result);
		}
		else if (AppId != nullptr && AppId->GetType() == EOnlineKeyValuePairDataType::String)
		{
			AppId->GetValue(Result);
		}
		else if (LegacyId != nullptr && LegacyId->GetType() == EOnlineKeyValuePairDataType::String)
		{
			LegacyId->GetValue(Result);
		}
	}
	return Result;
}

const FSlateBrush* FFriendItem::GetPresenceBrush() const
{
	if (WebImageService.IsValid())
	{
		return WebImageService->GetBrush(GetAppId());
	}
	return nullptr;
}

FDateTime FFriendItem::GetLastSeen() const
{
	return FDateTime::UtcNow();
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
	return false;
}

bool FFriendItem::IsInParty() const
{
	TSharedPtr<IOnlinePartyJoinInfo> PartyInfo = GetPartyJoinInfo();
	return PartyInfo.IsValid();
}

bool FFriendItem::CanJoinParty() const
{
	return false;
}

bool FFriendItem::CanInvite() const
{
	return false;
}

TSharedPtr<const FUniqueNetId> FFriendItem::GetGameSessionId() const
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

TSharedPtr<IOnlinePartyJoinInfo> FFriendItem::GetPartyJoinInfo() const
{
	return nullptr;
}

const TSharedRef<const FUniqueNetId> FFriendItem::GetUniqueID() const
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

void FFriendItem::SetWebImageService(const TSharedRef<class FWebImageService>& InWebImageService)
{
	WebImageService = InWebImageService;
}

#undef LOCTEXT_NAMESPACE
