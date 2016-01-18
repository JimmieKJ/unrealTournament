// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "FriendsAndChatAnalytics.h"

class FFriendsAndChatAnalyticsImpl
	: public FFriendsAndChatAnalytics
{
public:

	virtual ~FFriendsAndChatAnalyticsImpl() { }

	virtual void SetProvider(const TSharedPtr<IAnalyticsProvider>& InAnalyticsProvider) override
	{
		AnalyticsProvider = InAnalyticsProvider;
	}

	virtual void FireEvent_AcceptGameInvite(const FUniqueNetId& LocalUserId, const FUniqueNetId& ToUser) const override
	{
		static const FString EventName = TEXT("Social.GameInvite.Accept");

		if (AnalyticsProvider.IsValid())
		{
			if (LocalUserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), LocalUserId.ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Friend"), ToUser.ToString()));
				AddPresenceAttributes(LocalUserId, Attributes);
				AnalyticsProvider->RecordEvent(EventName, Attributes);
			}
		}
	}

	virtual void FireEvent_RejectGameInvite(const FUniqueNetId& LocalUserId, const FUniqueNetId& ToUser) const override
	{

		static const FString EventName =  TEXT("Social.GameInvite.Reject");
		if (AnalyticsProvider.IsValid())
		{
			if (LocalUserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), LocalUserId.ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Friend"), ToUser.ToString()));
				AddPresenceAttributes(LocalUserId, Attributes);
				AnalyticsProvider->RecordEvent(EventName, Attributes);
			}
		}
	}

	virtual void FireEvent_SendGameInvite(const FUniqueNetId& LocalUserId, const FUniqueNetId& ToUser) const override
	{
		static const FString EventName = TEXT("Social.GameInvite.Send");

		if (AnalyticsProvider.IsValid())
		{
			if (LocalUserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), LocalUserId.ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Friend"), ToUser.ToString()));
				AddPresenceAttributes(LocalUserId, Attributes);
				AnalyticsProvider->RecordEvent(EventName, Attributes);
			}
		}
	}

	virtual void FireEvent_FriendRequestAccepted(const FUniqueNetId& LocalUserId, const IFriendItem& Friend) const override
	{
		static const FString EventName = TEXT("Social.FriendAction.Accept");
	
		if (AnalyticsProvider.IsValid())
		{
			if (LocalUserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), LocalUserId.ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Friend"), Friend.GetUniqueID()->ToString()));
				AddPresenceAttributes(LocalUserId, Attributes);
				AnalyticsProvider->RecordEvent(EventName, Attributes);
			}
		}
	}

	virtual void FireEvent_FriendRemoved(const FUniqueNetId& LocalUserId, const IFriendItem& Friend, const FString& RemoveReason) const override
	{
		static const FString EventName = TEXT("Social.FriendRemoved");
		if (AnalyticsProvider.IsValid())
		{
			if (LocalUserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), LocalUserId.ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Friend"), Friend.GetUniqueID()->ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("RemoveReason"), RemoveReason));
				AddPresenceAttributes(LocalUserId, Attributes);
				AnalyticsProvider->RecordEvent(EventName, Attributes);
			}
		}
	}

	virtual void FireEvent_PlayerBlocked(const FUniqueNetId& LocalUserId, const IFriendItem& Player) const override
	{
		static const FString EventName = TEXT("Social.PlayerBlocked");
		if (AnalyticsProvider.IsValid())
		{
			if (LocalUserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), LocalUserId.ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Player"), Player.GetUniqueID()->ToString()));
				AddPresenceAttributes(LocalUserId, Attributes);
				AnalyticsProvider->RecordEvent(EventName, Attributes);
			}
		}
	}

	virtual void FireEvent_PlayerUnblocked(const FUniqueNetId& LocalUserId, const IFriendItem& Player) const override
	{
		static const FString EventName = TEXT("Social.PlayerUnblocked");
		if (AnalyticsProvider.IsValid())
		{
			if (LocalUserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), LocalUserId.ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Player"), Player.GetUniqueID()->ToString()));
				AddPresenceAttributes(LocalUserId, Attributes);
				AnalyticsProvider->RecordEvent(EventName, Attributes);
			}
		}
	}

	virtual void FireEvent_AddFriend(const FUniqueNetId& LocalUserId, const FString& FriendName, const FUniqueNetId& FriendId, EFindFriendResult::Type Result, bool bRecentPlayer) const override
	{
		static const FString EventName = TEXT("Social.AddFriend");

		if (AnalyticsProvider.IsValid())
		{
			if (LocalUserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), LocalUserId.ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Friend"), FriendId.ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("FriendName"), FriendName));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Result"), EFindFriendResult::ToString(Result)));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("bRecentPlayer"), bRecentPlayer));
				AddPresenceAttributes(LocalUserId, Attributes);
				AnalyticsProvider->RecordEvent(EventName, Attributes);
			}
		}
	}

	virtual void FireEvent_RecordToggleChat(const FUniqueNetId& LocalUserId, const FString& Channel, bool bEnabled) const override
	{
		static const FString EventName = TEXT("Social.Chat.Toggle");
		if (AnalyticsProvider.IsValid())
		{
			if (LocalUserId.IsValid())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("User"), LocalUserId.ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Channel"), Channel));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("bEnabled"), bEnabled));
				AddPresenceAttributes(LocalUserId, Attributes);
				AnalyticsProvider->RecordEvent(EventName, Attributes);
			}
		}
	}

	virtual void RecordPrivateChat(const FString& ToUser) override
	{
		int32& Count = PrivateChatCounts.FindOrAdd(ToUser);
		Count += 1;
	}

	virtual void RecordChannelChat(const FString& ToChannel) override
	{
		int32& Count = ChannelChatCounts.FindOrAdd(ToChannel);
		Count += 1;
	}

	virtual void FireEvent_FlushChatStats() override
	{
		if (AnalyticsProvider.IsValid())
		{
			IOnlineIdentityPtr OnlineIdentity = Online::GetIdentityInterface(MCP_SUBSYSTEM);
			if (OnlineIdentity.IsValid())
			{
				TSharedPtr<const FUniqueNetId> UserId = OnlineIdentity->GetUniquePlayerId(0);
				if (UserId.IsValid())
				{
					auto RecordSocialChatCountsEvents = [=](const TMap<FString, int32>& ChatCounts, const FString& ChatType)
					{
						if (ChatCounts.Num())
						{
							TArray<FAnalyticsEventAttribute> Attributes;
							for (const auto& Pair : ChatCounts)
							{
								Attributes.Empty(3);
								Attributes.Emplace(TEXT("Name"), Pair.Key);
								Attributes.Emplace(TEXT("Type"), ChatType);
								Attributes.Emplace(TEXT("Count"), Pair.Value);
								AnalyticsProvider->RecordEvent("Social.Chat.Counts.2", Attributes);
							}
						}
					};

					RecordSocialChatCountsEvents(ChannelChatCounts, TEXT("Channel"));
					RecordSocialChatCountsEvents(PrivateChatCounts, TEXT("Private"));
				}
			}
		}
		ChannelChatCounts.Empty();
		PrivateChatCounts.Empty();
	}

private: // functions

	void AddPresenceAttributes(const FUniqueNetId& UserId, TArray<FAnalyticsEventAttribute>& Attributes) const
	{
		IOnlinePresencePtr OnlinePresence = Online::GetPresenceInterface(MCP_SUBSYSTEM);
		if (OnlinePresence.IsValid())
		{
			TSharedPtr<FOnlineUserPresence> Presence;
			OnlinePresence->GetCachedPresence(UserId, Presence);
			if (Presence.IsValid())
			{
				FVariantData* AppIdData = Presence->Status.Properties.Find(DefaultAppIdKey);
				if (AppIdData != nullptr)
				{
					Attributes.Add(FAnalyticsEventAttribute(TEXT("ClientId"), AppIdData->ToString()));
				}
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Status"), Presence->Status.StatusStr));
			}
		}
	}

	FFriendsAndChatAnalyticsImpl()
	{}

private: // data

	// cached analytics provider for pushing events
	TSharedPtr<IAnalyticsProvider> AnalyticsProvider;

	// map of chat id to # of messages sent
	TMap<FString, int32> PrivateChatCounts;
	TMap<FString, int32> ChannelChatCounts;

	friend FFriendsAndChatAnalyticsFactory;
};

TSharedRef< FFriendsAndChatAnalytics > FFriendsAndChatAnalyticsFactory::Create()
{
	TSharedRef< FFriendsAndChatAnalyticsImpl > Instance = MakeShareable(new FFriendsAndChatAnalyticsImpl());
	return Instance;
}
