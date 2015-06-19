// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "OnlineChatInterface.h"
#include "ChatItemViewModel.h"

#define LOCTEXT_NAMESPACE "FriendsMessageManager"
// Message expiry time for different message types
static const int32 GlobalMessageLifetime = 5 * 60;  // 5 min
static const int32 PartyMessageLifetime = 5 * 60;  // 5 min
static const int32 WhisperMessageLifetime = 5 * 60;  // 5 min
static const int32 MessageStore = 200;
static const int32 GlobalMaxStore = 100;
static const int32 WhisperMaxStore = 100;
static const int32 PartyMaxStore = 100;

class FFriendsMessageManagerImpl
	: public FFriendsMessageManager
{
public:

	virtual void LogIn() override
	{
		Initialize();

		if (OnlineSub != nullptr &&
			OnlineSub->GetIdentityInterface().IsValid())
		{
			LoggedInUser = OnlineSub->GetIdentityInterface()->GetUniquePlayerId(0);
		}

		for (auto RoomName : RoomJoins)
		{
			JoinPublicRoom(RoomName);
		}
	}

	virtual void LogOut() override
	{
		if (OnlineSub != nullptr &&
			OnlineSub->GetChatInterface().IsValid() &&
			LoggedInUser.IsValid())
		{
			// exit out of any rooms that we're in
			TArray<FChatRoomId> JoinedRooms;
			OnlineSub->GetChatInterface()->GetJoinedRooms(*LoggedInUser, JoinedRooms);
			for (auto RoomId : JoinedRooms)
			{
				OnlineSub->GetChatInterface()->ExitRoom(*LoggedInUser, RoomId);
			}
		}
		LoggedInUser.Reset();
		UnInitialize();
	}

	virtual const TArray<TSharedRef<FFriendChatMessage> >& GetMessages() const override
	{
		return ReceivedMessages;
	}

	virtual bool SendRoomMessage(const FString& RoomName, const FString& MsgBody) override
	{
		if (OnlineSub != nullptr &&
			LoggedInUser.IsValid())
		{
			IOnlineChatPtr ChatInterface = OnlineSub->GetChatInterface();
			if (ChatInterface.IsValid())
			{
				if (!RoomName.IsEmpty())
				{
					TSharedPtr<FChatRoomInfo> RoomInfo = ChatInterface->GetRoomInfo(*LoggedInUser, FChatRoomId(RoomName));
					if (RoomInfo.IsValid() &&
						RoomInfo->IsJoined())
					{
						return ChatInterface->SendRoomChat(*LoggedInUser, FChatRoomId(RoomName), MsgBody);
					}
				}
				else
				{
					// send to all joined rooms
					bool bAbleToSend = false;
					TArray<FChatRoomId> JoinedRooms;
					OnlineSub->GetChatInterface()->GetJoinedRooms(*LoggedInUser, JoinedRooms);
					for (auto RoomId : JoinedRooms)
					{
						if (ChatInterface->SendRoomChat(*LoggedInUser, RoomId, MsgBody))
						{
							bAbleToSend = true;
						}
					}
					return bAbleToSend;
				}

			}
		}
		return false;
	}

	virtual bool SendPrivateMessage(TSharedPtr<FUniqueNetId> UserID, const FText MessageText) override
	{
		if (OnlineSub != nullptr &&
			LoggedInUser.IsValid())
		{
			IOnlineChatPtr ChatInterface = OnlineSub->GetChatInterface();
			if(ChatInterface.IsValid())
			{
				TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
				ChatItem->FromName = FText::FromString(OnlineSub->GetIdentityInterface()->GetPlayerNickname(*LoggedInUser.Get()));
				TSharedPtr<IFriendItem> FoundFriend = FFriendsAndChatManager::Get()->FindUser(*UserID.Get());
				ChatItem->ToName = FText::FromString(*FoundFriend->GetName());
				ChatItem->Message = MessageText;
				ChatItem->MessageType = EChatMessageType::Whisper;
				ChatItem->MessageTime = FDateTime::UtcNow();
				ChatItem->ExpireTime = FDateTime::UtcNow() + FTimespan::FromSeconds(WhisperMessageLifetime);
				ChatItem->bIsFromSelf = true;
				ChatItem->SenderId = LoggedInUser;
				ChatItem->RecipientId = UserID;
				AddMessage(ChatItem.ToSharedRef());
				return ChatInterface->SendPrivateChat(*LoggedInUser, *UserID.Get(), MessageText.ToString());
			}
		}
		return false;
	}

	virtual void InsertNetworkMessage(const FString& MsgBody) override
	{
		TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
		ChatItem->FromName = FText::FromString("Game");
		ChatItem->Message = FText::FromString(MsgBody);
		ChatItem->MessageType = EChatMessageType::Party;
		ChatItem->MessageTime = FDateTime::UtcNow();
		ChatItem->ExpireTime = FDateTime::UtcNow() + FTimespan::FromSeconds(PartyMessageLifetime);
		ChatItem->bIsFromSelf = false;
		PartyMessagesCount++;
		AddMessage(ChatItem.ToSharedRef());
	}

	virtual void JoinPublicRoom(const FString& RoomName) override
	{
		if (LoggedInUser.IsValid())
		{
			if (OnlineSub != nullptr &&
				OnlineSub->GetChatInterface().IsValid())
			{
				// join the room to start receiving messages from it
				FString NickName = OnlineSub->GetIdentityInterface()->GetPlayerNickname(*LoggedInUser);
				OnlineSub->GetChatInterface()->JoinPublicRoom(*LoggedInUser, FChatRoomId(RoomName), NickName);
			}
		}
		RoomJoins.AddUnique(RoomName);
	}

	DECLARE_DERIVED_EVENT(FFriendsMessageManagerImpl, FFriendsMessageManager::FOnChatMessageReceivedEvent, FOnChatMessageReceivedEvent)
	virtual FOnChatMessageReceivedEvent& OnChatMessageRecieved() override
	{
		return MessageReceivedEvent;
	}
	DECLARE_DERIVED_EVENT(FFriendsMessageManagerImpl, FFriendsMessageManager::FOnChatPublicRoomJoinedEvent, FOnChatPublicRoomJoinedEvent)
	virtual FOnChatPublicRoomJoinedEvent& OnChatPublicRoomJoined() override
	{
		return PublicRoomJoinedEvent;
	}
	DECLARE_DERIVED_EVENT(FFriendsMessageManagerImpl, FFriendsMessageManager::FOnChatPublicRoomExitedEvent, FOnChatPublicRoomExitedEvent)
	virtual FOnChatPublicRoomExitedEvent& OnChatPublicRoomExited() override
	{
		return PublicRoomExitedEvent;
	}

	~FFriendsMessageManagerImpl()
	{
	}

private:
	FFriendsMessageManagerImpl( )
		: OnlineSub(nullptr)
		, bEnableEnterExitMessages(false)
	{
	}

	void Initialize()
	{
		// Clear existing data
		LogOut();

		GlobalMessagesCount = 0;
		WhisperMessagesCount = 0;
		PartyMessagesCount = 0;
		ReceivedMessages.Empty();

		OnlineSub = IOnlineSubsystem::Get( TEXT( "MCP" ) );

		if (OnlineSub != nullptr)
		{
			IOnlineChatPtr ChatInterface = OnlineSub->GetChatInterface();
			if (ChatInterface.IsValid())
			{
				OnChatRoomJoinPublicDelegate         = FOnChatRoomJoinPublicDelegate        ::CreateSP(this, &FFriendsMessageManagerImpl::OnChatRoomJoinPublic);
				OnChatRoomExitDelegate               = FOnChatRoomExitDelegate              ::CreateSP(this, &FFriendsMessageManagerImpl::OnChatRoomExit);
				OnChatRoomMemberJoinDelegate         = FOnChatRoomMemberJoinDelegate        ::CreateSP(this, &FFriendsMessageManagerImpl::OnChatRoomMemberJoin);
				OnChatRoomMemberExitDelegate         = FOnChatRoomMemberExitDelegate        ::CreateSP(this, &FFriendsMessageManagerImpl::OnChatRoomMemberExit);
				OnChatRoomMemberUpdateDelegate       = FOnChatRoomMemberUpdateDelegate      ::CreateSP(this, &FFriendsMessageManagerImpl::OnChatRoomMemberUpdate);
				OnChatRoomMessageReceivedDelegate    = FOnChatRoomMessageReceivedDelegate   ::CreateSP(this, &FFriendsMessageManagerImpl::OnChatRoomMessageReceived);
				OnChatPrivateMessageReceivedDelegate = FOnChatPrivateMessageReceivedDelegate::CreateSP(this, &FFriendsMessageManagerImpl::OnChatPrivateMessageReceived);

				OnChatRoomJoinPublicDelegateHandle         = ChatInterface->AddOnChatRoomJoinPublicDelegate_Handle        (OnChatRoomJoinPublicDelegate);
				OnChatRoomExitDelegateHandle               = ChatInterface->AddOnChatRoomExitDelegate_Handle              (OnChatRoomExitDelegate);
				OnChatRoomMemberJoinDelegateHandle         = ChatInterface->AddOnChatRoomMemberJoinDelegate_Handle        (OnChatRoomMemberJoinDelegate);
				OnChatRoomMemberExitDelegateHandle         = ChatInterface->AddOnChatRoomMemberExitDelegate_Handle        (OnChatRoomMemberExitDelegate);
				OnChatRoomMemberUpdateDelegateHandle       = ChatInterface->AddOnChatRoomMemberUpdateDelegate_Handle      (OnChatRoomMemberUpdateDelegate);
				OnChatRoomMessageReceivedDelegateHandle    = ChatInterface->AddOnChatRoomMessageReceivedDelegate_Handle   (OnChatRoomMessageReceivedDelegate);
				OnChatPrivateMessageReceivedDelegateHandle = ChatInterface->AddOnChatPrivateMessageReceivedDelegate_Handle(OnChatPrivateMessageReceivedDelegate);
			}
			IOnlinePresencePtr PresenceInterface = OnlineSub->GetPresenceInterface();
			if (PresenceInterface.IsValid())
			{
				OnPresenceReceivedDelegate = FOnPresenceReceivedDelegate::CreateSP(this, &FFriendsMessageManagerImpl::OnPresenceReceived);
				OnPresenceReceivedDelegateHandle = PresenceInterface->AddOnPresenceReceivedDelegate_Handle(OnPresenceReceivedDelegate);
			}
		}
	}

	void UnInitialize()
	{
		if (OnlineSub != nullptr)
		{
			IOnlineChatPtr ChatInterface = OnlineSub->GetChatInterface();
			if( ChatInterface.IsValid())
			{
				ChatInterface->ClearOnChatRoomJoinPublicDelegate_Handle        (OnChatRoomJoinPublicDelegateHandle);
				ChatInterface->ClearOnChatRoomExitDelegate_Handle              (OnChatRoomExitDelegateHandle);
				ChatInterface->ClearOnChatRoomMemberJoinDelegate_Handle        (OnChatRoomMemberJoinDelegateHandle);
				ChatInterface->ClearOnChatRoomMemberExitDelegate_Handle        (OnChatRoomMemberExitDelegateHandle);
				ChatInterface->ClearOnChatRoomMemberUpdateDelegate_Handle      (OnChatRoomMemberUpdateDelegateHandle);
				ChatInterface->ClearOnChatRoomMessageReceivedDelegate_Handle   (OnChatRoomMessageReceivedDelegateHandle);
				ChatInterface->ClearOnChatPrivateMessageReceivedDelegate_Handle(OnChatPrivateMessageReceivedDelegateHandle);
			}
			IOnlinePresencePtr PresenceInterface = OnlineSub->GetPresenceInterface();
			if (PresenceInterface.IsValid())
			{
				PresenceInterface->ClearOnPresenceReceivedDelegate_Handle(OnPresenceReceivedDelegateHandle);
			}
		}
		OnlineSub = nullptr;
	}

	void OnChatRoomJoinPublic(const FUniqueNetId& UserId, const FChatRoomId& ChatRoomID, bool bWasSuccessful, const FString& Error)
	{
		if (bWasSuccessful)
		{
			OnChatPublicRoomJoined().Broadcast(ChatRoomID);
		}
	}

	void OnChatRoomExit(const FUniqueNetId& UserId, const FChatRoomId& ChatRoomID, bool bWasSuccessful, const FString& Error)
	{
		if (bWasSuccessful)
		{
			OnChatPublicRoomExited().Broadcast(ChatRoomID);
		}		
	}

	void OnChatRoomMemberJoin(const FUniqueNetId& UserId, const FChatRoomId& ChatRoomID, const FUniqueNetId& MemberId)
	{
		if (bEnableEnterExitMessages &&
			LoggedInUser.IsValid() &&
			*LoggedInUser != MemberId)
		{
			TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
			TSharedPtr<FChatRoomMember> ChatMember = OnlineSub->GetChatInterface()->GetMember(UserId, ChatRoomID, MemberId);
			if (ChatMember.IsValid())
			{
				ChatItem->FromName = FText::FromString(*ChatMember->GetNickname());
			}
			ChatItem->Message = FText::FromString(TEXT("entered room"));
			ChatItem->MessageType = EChatMessageType::Global;
			ChatItem->MessageTime = FDateTime::UtcNow();
			ChatItem->ExpireTime = FDateTime::UtcNow() + GlobalMessageLifetime;
			ChatItem->bIsFromSelf = false;
			GlobalMessagesCount++;
			AddMessage(ChatItem.ToSharedRef());
		}
	}

	void OnChatRoomMemberExit(const FUniqueNetId& UserId, const FChatRoomId& ChatRoomID, const FUniqueNetId& MemberId)
	{
		if (bEnableEnterExitMessages &&
			LoggedInUser.IsValid() &&
			*LoggedInUser != MemberId)
		{
			TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
			TSharedPtr<FChatRoomMember> ChatMember = OnlineSub->GetChatInterface()->GetMember(UserId, ChatRoomID, MemberId);
			if (ChatMember.IsValid())
			{
				ChatItem->FromName = FText::FromString(*ChatMember->GetNickname());
			}
			ChatItem->Message = FText::FromString(TEXT("left room"));
			ChatItem->MessageType = EChatMessageType::Global;
			ChatItem->MessageTime = FDateTime::UtcNow();
			ChatItem->ExpireTime = FDateTime::UtcNow() + GlobalMessageLifetime;
			ChatItem->bIsFromSelf = false;
			GlobalMessagesCount++;
			AddMessage(ChatItem.ToSharedRef());
		}
	}

	void OnChatRoomMemberUpdate(const FUniqueNetId& UserId, const FChatRoomId& ChatRoomID, const FUniqueNetId& MemberId)
	{
	}

	void OnChatRoomMessageReceived(const FUniqueNetId& UserId, const FChatRoomId& ChatRoomID, const TSharedRef<FChatMessage>& ChatMessage)
	{
		TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());

		ChatItem->FromName = FText::FromString(*ChatMessage->GetNickname());
		ChatItem->Message = FText::FromString(*ChatMessage->GetBody());
		ChatItem->MessageType = EChatMessageType::Global;
		ChatItem->MessageTime = ChatMessage->GetTimestamp();
		ChatItem->ExpireTime = ChatMessage->GetTimestamp() + FTimespan::FromSeconds(GlobalMessageLifetime);
		ChatItem->bIsFromSelf = *ChatMessage->GetUserId() == *LoggedInUser;
		ChatItem->SenderId = ChatMessage->GetUserId();
		ChatItem->RecipientId = nullptr;
		ChatItem->MessageRef = ChatMessage;
		GlobalMessagesCount++;
		AddMessage(ChatItem.ToSharedRef());
	}

	void OnChatPrivateMessageReceived(const FUniqueNetId& UserId, const TSharedRef<FChatMessage>& ChatMessage)
	{
		TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
		TSharedPtr<IFriendItem> FoundFriend = FFriendsAndChatManager::Get()->FindUser(ChatMessage->GetUserId());
		// Ignore messages from unknown people
		if(FoundFriend.IsValid())
		{
			ChatItem->FromName = FText::FromString(*FoundFriend->GetName());
			ChatItem->SenderId = FoundFriend->GetUniqueID();
			ChatItem->bIsFromSelf = false;
			ChatItem->RecipientId = LoggedInUser;
			ChatItem->Message = FText::FromString(*ChatMessage->GetBody());
			ChatItem->MessageType = EChatMessageType::Whisper;
			ChatItem->MessageTime = ChatMessage->GetTimestamp();
			ChatItem->ExpireTime = ChatMessage->GetTimestamp() + FTimespan::FromSeconds(WhisperMessageLifetime);
			ChatItem->MessageRef = ChatMessage;
			WhisperMessagesCount++;
			AddMessage(ChatItem.ToSharedRef());

			// Inform listers that we have received a chat message
			FFriendsAndChatManager::Get()->SendChatMessageReceivedEvent(ChatItem->MessageType, FoundFriend);
		}
	}

	void OnPresenceReceived(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& Presence)
	{
		if (LoggedInUser.IsValid() &&
			*LoggedInUser == UserId)
		{
			if (Presence->bIsOnline)
			{
				for (auto RoomName : RoomJoins)
				{
					JoinPublicRoom(RoomName);
				}
			}
		}
	}

	void AddMessage(TSharedRef< FFriendChatMessage > NewMessage)
	{
		//TSharedRef<FChatItemViewModel> NewMessage = FChatItemViewModelFactory::Create(ChatItem);
		if(ReceivedMessages.Add(NewMessage) > MessageStore)
		{
			bool bGlobalTimeFound = false;
			bool bPartyTimeFound = false;
			bool bWhisperFound = false;
			FDateTime CurrentTime = FDateTime::UtcNow();
			for(int32 Index = 0; Index < ReceivedMessages.Num(); Index++)
			{
				TSharedRef<FFriendChatMessage> Message = ReceivedMessages[Index];
				if (Message->ExpireTime < CurrentTime)
				{
					RemoveMessage(Message);
					Index--;
				}
				else
				{
					switch (Message->MessageType)
					{
						case EChatMessageType::Global :
						{
							if(GlobalMessagesCount > GlobalMaxStore)
							{
								RemoveMessage(Message);
								Index--;
							}
							else
							{
								bGlobalTimeFound = true;
							}
						}
						break;
						case EChatMessageType::Party :
						{
							if(PartyMessagesCount > PartyMaxStore)
							{
								RemoveMessage(Message);
								Index--;
							}
							else
							{
								bPartyTimeFound = true;
							}
						}
						break;
						case EChatMessageType::Whisper :
						{
							if(WhisperMessagesCount > WhisperMaxStore)
							{
								RemoveMessage(Message);
								Index--;
							}
							else
							{
								bWhisperFound = true;
							}
						}
						break;
					}
				}
				if(ReceivedMessages.Num() < MessageStore || (bPartyTimeFound && bGlobalTimeFound && bWhisperFound))
				{
					break;
				}
			}
		}
		OnChatMessageRecieved().Broadcast(NewMessage);
	}

	void RemoveMessage(TSharedRef<FFriendChatMessage> Message)
	{
		switch(Message->MessageType)
		{
			case EChatMessageType::Global : GlobalMessagesCount--; break;
			case EChatMessageType::Party : PartyMessagesCount--; break;
			case EChatMessageType::Whisper : WhisperMessagesCount--; break;
		}
		ReceivedMessages.Remove(Message);
	}
private:

	// Incoming delegates
	FOnChatRoomJoinPublicDelegate OnChatRoomJoinPublicDelegate;
	FOnChatRoomExitDelegate OnChatRoomExitDelegate;
	FOnChatRoomMemberJoinDelegate OnChatRoomMemberJoinDelegate;
	FOnChatRoomMemberExitDelegate OnChatRoomMemberExitDelegate;
	FOnChatRoomMemberUpdateDelegate OnChatRoomMemberUpdateDelegate;
	FOnChatRoomMessageReceivedDelegate OnChatRoomMessageReceivedDelegate;
	FOnChatPrivateMessageReceivedDelegate OnChatPrivateMessageReceivedDelegate;
	FOnPresenceReceivedDelegate OnPresenceReceivedDelegate;

	// Handles to the above registered delegates
	FDelegateHandle OnChatRoomJoinPublicDelegateHandle;
	FDelegateHandle OnChatRoomExitDelegateHandle;
	FDelegateHandle OnChatRoomMemberJoinDelegateHandle;
	FDelegateHandle OnChatRoomMemberExitDelegateHandle;
	FDelegateHandle OnChatRoomMemberUpdateDelegateHandle;
	FDelegateHandle OnChatRoomMessageReceivedDelegateHandle;
	FDelegateHandle OnChatPrivateMessageReceivedDelegateHandle;
	FDelegateHandle OnPresenceReceivedDelegateHandle;

	// Outgoing events
	FOnChatMessageReceivedEvent MessageReceivedEvent;
	FOnChatPublicRoomJoinedEvent PublicRoomJoinedEvent;
	FOnChatPublicRoomExitedEvent PublicRoomExitedEvent;

	IOnlineSubsystem* OnlineSub;
	TSharedPtr<FUniqueNetId> LoggedInUser;
	TArray<FString> RoomJoins;

	TArray<TSharedRef<FFriendChatMessage> > ReceivedMessages;

	int32 GlobalMessagesCount;
	int32 WhisperMessagesCount;
	int32 PartyMessagesCount;

	bool bEnableEnterExitMessages;

private:
	friend FFriendsMessageManagerFactory;
};

TSharedRef< FFriendsMessageManager > FFriendsMessageManagerFactory::Create()
{
	TSharedRef< FFriendsMessageManagerImpl > MessageManager(new FFriendsMessageManagerImpl());
	return MessageManager;
}

#undef LOCTEXT_NAMESPACE