// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "OnlineChatInterface.h"
#include "ChatItemViewModel.h"
#include "IChatCommunicationService.h"
#include "FriendsAndChatAnalytics.h"
#include "OSSScheduler.h"
#include "FriendsService.h"
#include "FriendsNavigationService.h"
#include "GameAndPartyService.h"
#include "ChatNotificationService.h"

#define LOCTEXT_NAMESPACE ""

// Message expiry time for different message types
static const int32 GlobalMessageLifetime = 5 * 60;  // 5 min
static const int32 GameMessageLifetime = 5 * 60;  // 5 min
static const int32 PartyMessageLifetime = 5 * 60;  // 5 min
static const int32 WhisperMessageLifetime = 5 * 60;  // 5 min
static const int32 MessageStore = 200;
static const int32 GlobalMaxStore = 100;
static const int32 WhisperMaxStore = 100;
static const int32 GameMaxStore = 100;
static const int32 PartyMaxStore = 100;
static const int32 AdminMaxStore = 100;
static const int32 LiveStreamMaxStore = 100;

class FMessageServiceImpl
	: public FMessageService
{
public:

	virtual void Login() override
	{
		// Clear existing data
		Logout();

		Initialize();
		if (OSSScheduler->GetOnlineIdentity().IsValid())
		{
			LoggedInUser = OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(0);
		}

		for (auto RoomName : RoomJoins)
		{
			JoinPublicRoom(RoomName);
		}
	}

	virtual void Logout() override
	{
		if (OSSScheduler->GetChatInterface().IsValid() &&
			LoggedInUser.IsValid())
		{
			// exit out of any rooms that we're in
			TArray<FChatRoomId> JoinedRooms;
			OSSScheduler->GetChatInterface()->GetJoinedRooms(*LoggedInUser, JoinedRooms);
			for (auto RoomId : JoinedRooms)
			{
				OSSScheduler->GetChatInterface()->ExitRoom(*LoggedInUser, RoomId);
			}
		}
		RoomJoins.Empty();
		LoggedInUser.Reset();
		bJoinedGlobalChat = false;
		UnInitialize();
	}

	virtual const TArray<TSharedRef<FFriendChatMessage> >& GetMessages() const override
	{
		return ReceivedMessages;
	}

	virtual bool SendRoomMessage(const FString& RoomName, const FString& MsgBody) override
	{
		bool bMessageSent = false;
		if (LoggedInUser.IsValid() && OSSScheduler->CanChatOnline())
		{
			IOnlineChatPtr ChatInterface = OSSScheduler->GetChatInterface();
			if (ChatInterface.IsValid())
			{
				if (!RoomName.IsEmpty())
				{
					TSharedPtr<FChatRoomInfo> RoomInfo = ChatInterface->GetRoomInfo(*LoggedInUser, FChatRoomId(RoomName));
					if (RoomInfo.IsValid() &&
						RoomInfo->IsJoined())
					{
						RegisterMessageSent();
						bMessageSent = ChatInterface->SendRoomChat(*LoggedInUser, FChatRoomId(RoomName), MsgBody);
					}
				}
				else
				{
					// send to all joined rooms
					bool bAbleToSend = false;
					TArray<FChatRoomId> JoinedRooms;
					OSSScheduler->GetChatInterface()->GetJoinedRooms(*LoggedInUser, JoinedRooms);
					for (auto RoomId : JoinedRooms)
					{
						if (RoomJoins.Contains(RoomId))
						{
							if (ChatInterface->SendRoomChat(*LoggedInUser, RoomId, MsgBody))
							{
								bAbleToSend = true;
							}
						}
					}

					if(bAbleToSend)
					{
						bMessageSent = true;
						RegisterMessageSent();
					}
				}
			}
		}

		if(!bMessageSent)
		{
			TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
			static const FText FailedToSend = LOCTEXT("CouldNotSend", "Unable to send chat message");
			ChatItem->Message = FailedToSend;
			AddMessage(ChatItem.ToSharedRef());

		}
		return bMessageSent;
	}

	virtual bool SendPrivateMessage(TSharedPtr<IFriendItem> Friend, const FText MessageText) override
	{
	bool bMessageSent = false;
		if (LoggedInUser.IsValid() && OSSScheduler->CanChatOnline())
		{
			IOnlineChatPtr ChatInterface = OSSScheduler->GetChatInterface();
			if(ChatInterface.IsValid() && Friend.IsValid() && Friend->GetInviteStatus() == EInviteStatus::Accepted)
			{
				TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
				ChatItem->FromName = FText::FromString(OSSScheduler->GetPlayerNickname(*LoggedInUser.Get()));
				ChatItem->ToName = FText::FromString(*Friend->GetName());
				ChatItem->Message = MessageText;
				ChatItem->MessageType = EChatMessageType::Whisper;
				ChatItem->MessageTime = FDateTime::Now();
				ChatItem->ExpireTime = FDateTime::Now() + FTimespan::FromSeconds(WhisperMessageLifetime);
				ChatItem->bIsFromSelf = true;
				ChatItem->SenderId = LoggedInUser;
				ChatItem->RecipientId = Friend->GetUniqueID();
				AddMessage(ChatItem.ToSharedRef());
				
				OSSScheduler->GetAnalytics()->RecordPrivateChat(Friend->GetUniqueID()->ToString());
				RegisterMessageSent();

				bMessageSent = ChatInterface->SendPrivateChat(*LoggedInUser, *ChatItem->RecipientId.Get(), MessageText.ToString());
			}
		}

		if(!bMessageSent)
		{
			TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
			static const FText FailedToSend = LOCTEXT("CouldNotSend", "Unable to send chat message");
			ChatItem->Message = FailedToSend;
			AddMessage(ChatItem.ToSharedRef());

		}

		return bMessageSent;
	}

	virtual void SendGameMessage(const FString& MsgBody) override
	{
		OnNetworkMessageSentEvent().Broadcast(MsgBody);
	}

	virtual void InsertNetworkMessage(const FString& MsgBody) override
	{
		TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
		ChatItem->FromName = FText::FromString("Game");
		ChatItem->Message = FText::FromString(MsgBody);
		ChatItem->MessageType = EChatMessageType::Game;
		ChatItem->MessageTime = FDateTime::Now();
		ChatItem->ExpireTime = FDateTime::Now() + FTimespan::FromSeconds(GameMessageLifetime);
		ChatItem->bIsFromSelf = false;
		GameMessagesCount++;
		AddMessage(ChatItem.ToSharedRef());
	}

	virtual void InsertAdminMessage(const FString& MsgBody) override
	{
		TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
		ChatItem->FromName = FText::FromString("Admin");
		ChatItem->Message = FText::FromString(MsgBody);
		ChatItem->MessageType = EChatMessageType::Admin;
		ChatItem->MessageTime = FDateTime::Now();
		ChatItem->ExpireTime = FDateTime::Now() + FTimespan::FromSeconds(GameMessageLifetime);
		ChatItem->bIsFromSelf = false;
		AdminMessagesCount++;
		AddMessage(ChatItem.ToSharedRef());
	}
	
	virtual void InsertLiveStreamMessage(const FString& MsgBody) override
	{
		if (OSSScheduler->CanChatOnline())
		{
			TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
			ChatItem->FromName = FText::FromString("LiveStream");
			ChatItem->Message = FText::FromString(MsgBody);
			ChatItem->MessageType = EChatMessageType::LiveStreaming;
			ChatItem->MessageTime = FDateTime::Now();
			ChatItem->ExpireTime = FDateTime::Now() + FTimespan::FromSeconds(GameMessageLifetime);
			ChatItem->bIsFromSelf = false;
			LiveStreamMessagesCount++;
			AddMessage(ChatItem.ToSharedRef());
		}
	}

	virtual void EnableChatThrottle(int32 TicketCount, float TicketInterval) override
	{
		DisableChatThrottle();
		ChatThrottleEnabled = true;
		ChatTicketCount = TicketCount;
		ChatTicketLimit = TicketCount;
		if (!TickDelegate.IsBound())
		{
			TickDelegate = FTickerDelegate::CreateSP(this, &FMessageServiceImpl::HandleTick);
		}
		TickerHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, TicketInterval);
	}

	virtual void DisableChatThrottle() override
	{
		ChatThrottleEnabled = false;
		if (TickerHandle.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();
		}
	}

	virtual void JoinPublicRoom(const FString& RoomName) override
	{
		if (LoggedInUser.IsValid())
		{
			if (OSSScheduler->GetChatInterface().IsValid())
			{
				// join the room to start receiving messages from it
				FString NickName = OSSScheduler->GetPlayerNickname(*LoggedInUser);
				OSSScheduler->GetChatInterface()->JoinPublicRoom(*LoggedInUser, FChatRoomId(RoomName), NickName);
			}
		}
		RoomJoins.AddUnique(RoomName);
	}

	virtual EOnlinePresenceState::Type GetOnlineStatus() override
	{
		return OSSScheduler->GetOnlineStatus();
	}

	virtual bool IsThrottled() override
	{
		return ChatThrottleEnabled && ChatTicketCount < 0;
	}

	virtual void RegisterGameMessageSent()
	{
		RegisterMessageSent();
	}

	virtual TSharedPtr<class FFriendsAndChatAnalytics> GetAnalytics() const override
	{
		return OSSScheduler->GetAnalytics();
	}

	virtual bool IsInGlobalChat() const override
	{
		return bJoinedGlobalChat;
	}

	virtual bool OpenGlobalChat() override
	{
		// All this does now is send an analytics event
		if (IsInGlobalChat())
		{
			if (OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex).IsValid())
			{
				OSSScheduler->GetAnalytics()->FireEvent_RecordToggleChat(*OSSScheduler->GetOnlineIdentity()->GetUniquePlayerId(LocalControllerIndex), TEXT("Global"), true);
				return true;
			}
		}
		return false;
	}

	DECLARE_DERIVED_EVENT(FMessageServiceImpl, IChatCommunicationService::FOnChatMessageAddedEvent, FOnChatMessageAddedEvent)
	virtual FOnChatMessageAddedEvent& OnChatMessageAdded() override
	{
		return MessageAddedEvent;
	}

	DECLARE_DERIVED_EVENT(FMessageServiceImpl, IChatCommunicationService::FOnChatMessageReceivedEvent, FOnChatMessageReceivedEvent)
	virtual FOnChatMessageReceivedEvent& OnChatMessageRecieved() override
	{
		return MessageReceivedEvent;
	}

	DECLARE_DERIVED_EVENT(FMessageServiceImpl, IChatCommunicationService::FOnFriendsSendNetworkMessageEvent, FOnFriendsSendNetworkMessageEvent);
	virtual FOnFriendsSendNetworkMessageEvent& OnNetworkMessageSentEvent() override
	{
		return FriendsSendNetworkMessageEvent;
	}

	DECLARE_DERIVED_EVENT(FMessageServiceImpl, FMessageService::FOnChatPublicRoomJoinedEvent, FOnChatPublicRoomJoinedEvent)
	virtual FOnChatPublicRoomJoinedEvent& OnChatPublicRoomJoined() override
	{
		return PublicRoomJoinedEvent;
	}

	DECLARE_DERIVED_EVENT(FMessageServiceImpl, FMessageService::FOnChatPublicRoomExitedEvent, FOnChatPublicRoomExitedEvent)
	virtual FOnChatPublicRoomExitedEvent& OnChatPublicRoomExited() override
	{
		return PublicRoomExitedEvent;
	}

	~FMessageServiceImpl()
	{
		DisableChatThrottle();
	}

private:

	void Initialize()
	{
		GlobalMessagesCount = 0;
		WhisperMessagesCount = 0;
		GameMessagesCount = 0;
		PartyMessagesCount = 0;
		ReceivedMessages.Empty();
		bJoinedGlobalChat = false;

		IOnlineChatPtr ChatInterface = OSSScheduler->GetChatInterface();
		if (ChatInterface.IsValid())
		{
			OnChatRoomJoinPublicDelegate = FOnChatRoomJoinPublicDelegate::CreateSP(this, &FMessageServiceImpl::OnChatRoomJoinPublic);
			OnChatRoomExitDelegate = FOnChatRoomExitDelegate::CreateSP(this, &FMessageServiceImpl::OnChatRoomExit);
			OnChatRoomMemberJoinDelegate = FOnChatRoomMemberJoinDelegate::CreateSP(this, &FMessageServiceImpl::OnChatRoomMemberJoin);
			OnChatRoomMemberExitDelegate = FOnChatRoomMemberExitDelegate::CreateSP(this, &FMessageServiceImpl::OnChatRoomMemberExit);
			OnChatRoomMemberUpdateDelegate = FOnChatRoomMemberUpdateDelegate::CreateSP(this, &FMessageServiceImpl::OnChatRoomMemberUpdate);
			OnChatRoomMessageReceivedDelegate = FOnChatRoomMessageReceivedDelegate::CreateSP(this, &FMessageServiceImpl::OnChatRoomMessageReceived);
			OnChatPrivateMessageReceivedDelegate = FOnChatPrivateMessageReceivedDelegate::CreateSP(this, &FMessageServiceImpl::OnChatPrivateMessageReceived);

			OnChatRoomJoinPublicDelegateHandle = ChatInterface->AddOnChatRoomJoinPublicDelegate_Handle(OnChatRoomJoinPublicDelegate);
			OnChatRoomExitDelegateHandle = ChatInterface->AddOnChatRoomExitDelegate_Handle(OnChatRoomExitDelegate);
			OnChatRoomMemberJoinDelegateHandle = ChatInterface->AddOnChatRoomMemberJoinDelegate_Handle(OnChatRoomMemberJoinDelegate);
			OnChatRoomMemberExitDelegateHandle = ChatInterface->AddOnChatRoomMemberExitDelegate_Handle(OnChatRoomMemberExitDelegate);
			OnChatRoomMemberUpdateDelegateHandle = ChatInterface->AddOnChatRoomMemberUpdateDelegate_Handle(OnChatRoomMemberUpdateDelegate);
			OnChatRoomMessageReceivedDelegateHandle = ChatInterface->AddOnChatRoomMessageReceivedDelegate_Handle(OnChatRoomMessageReceivedDelegate);
			OnChatPrivateMessageReceivedDelegateHandle = ChatInterface->AddOnChatPrivateMessageReceivedDelegate_Handle(OnChatPrivateMessageReceivedDelegate);
		}
	}

	void UnInitialize()
	{
		IOnlineChatPtr ChatInterface = OSSScheduler->GetChatInterface();
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
	}

	void OnChatRoomJoinPublic(const FUniqueNetId& UserId, const FChatRoomId& ChatRoomID, bool bWasSuccessful, const FString& Error)
	{
		if (bWasSuccessful)
		{
			bJoinedGlobalChat = true;
			OnChatPublicRoomJoined().Broadcast(ChatRoomID);
		}
	}

	void OnChatRoomExit(const FUniqueNetId& UserId, const FChatRoomId& ChatRoomID, bool bWasSuccessful, const FString& Error)
	{
		if (bWasSuccessful)
		{
			bJoinedGlobalChat = false;
			OnChatPublicRoomExited().Broadcast(ChatRoomID);
		}		
	}

	void OnChatRoomMemberJoin(const FUniqueNetId& UserId, const FChatRoomId& ChatRoomID, const FUniqueNetId& MemberId)
	{
		if (bEnableEnterExitMessages &&
			LoggedInUser.IsValid() &&
			*LoggedInUser != MemberId &&
			OSSScheduler->CanChatOnline())
		{
			TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
			TSharedPtr<FChatRoomMember> ChatMember = OSSScheduler->GetChatInterface()->GetMember(UserId, ChatRoomID, MemberId);
			if (ChatMember.IsValid())
			{
				ChatItem->FromName = FText::FromString(*ChatMember->GetNickname());
			}
			ChatItem->Message = FText::FromString(TEXT("entered room"));
			ChatItem->MessageType = EChatMessageType::Global;
			ChatItem->MessageTime = FDateTime::Now();
			ChatItem->ExpireTime = FDateTime::Now() + GlobalMessageLifetime;
			ChatItem->bIsFromSelf = false;
			GlobalMessagesCount++;
			AddMessage(ChatItem.ToSharedRef());
		}
	}

	void OnChatRoomMemberExit(const FUniqueNetId& UserId, const FChatRoomId& ChatRoomID, const FUniqueNetId& MemberId)
	{
		if (bEnableEnterExitMessages &&
			LoggedInUser.IsValid() &&
			*LoggedInUser != MemberId &&
			OSSScheduler->CanChatOnline())
		{
			TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
			TSharedPtr<FChatRoomMember> ChatMember = OSSScheduler->GetChatInterface()->GetMember(UserId, ChatRoomID, MemberId);
			if (ChatMember.IsValid())
			{
				ChatItem->FromName = FText::FromString(*ChatMember->GetNickname());
			}
			ChatItem->Message = FText::FromString(TEXT("left room"));
			ChatItem->MessageType = EChatMessageType::Global;
			ChatItem->MessageTime = FDateTime::Now();
			ChatItem->ExpireTime = FDateTime::Now() + GlobalMessageLifetime;
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
		// Check if blocked
		if(OSSScheduler->CanChatOnline() && !IsPlayerMuted(ChatMessage->GetUserId()))
		{
			TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());

			{
				// Determine roomtype for the message.  
				FString GlobalChatRoomId;
				FChatRoomId PartyChatRoomId = GamePartyService->GetPartyChatRoomId();
				FChatRoomId TeamChatRoomID = GamePartyService->GetTeamChatRoomId(); 
				if (GetGlobalChatRoomId(GlobalChatRoomId) && ChatRoomID == GlobalChatRoomId)
				{
					ChatItem->MessageType = EChatMessageType::Global;
				}
				else if (!PartyChatRoomId.IsEmpty() && ChatRoomID == PartyChatRoomId)
				{
					ChatItem->MessageType = EChatMessageType::Party;
				}
				else if (!TeamChatRoomID.IsEmpty() && ChatRoomID == TeamChatRoomID)
				{
					ChatItem->MessageType = EChatMessageType::Team;
				}
				else
				{
					UE_LOG(LogOnline, Warning, TEXT("Received message for chatroom that didn't match global or party chatroom criteria %s"), *ChatRoomID);
				}
			}
			ChatItem->FromName = FText::FromString(*ChatMessage->GetNickname());
			ChatItem->Message = FText::FromString(*ChatMessage->GetBody());
			ChatItem->MessageTime = FDateTime::Now();
			ChatItem->ExpireTime = FDateTime::Now() + FTimespan::FromSeconds(GlobalMessageLifetime);
			ChatItem->bIsFromSelf = *ChatMessage->GetUserId() == *LoggedInUser;
			ChatItem->SenderId = ChatMessage->GetUserId();
			ChatItem->RecipientId = nullptr;
			ChatItem->MessageRef = ChatMessage;
			GlobalMessagesCount++;
			AddMessage(ChatItem.ToSharedRef());
		}
	}

	void OnChatPrivateMessageReceived(const FUniqueNetId& UserId, const TSharedRef<FChatMessage>& ChatMessage)
	{
		if(!IsPlayerMuted(ChatMessage->GetUserId()))
		{
			TSharedPtr< FFriendChatMessage > ChatItem = MakeShareable(new FFriendChatMessage());
			TSharedPtr<IFriendItem> FoundFriend = FriendsService->FindUser(ChatMessage->GetUserId());
			// Ignore messages from unknown people
			if(FoundFriend.IsValid() && FoundFriend->GetInviteStatus() == EInviteStatus::Accepted && OSSScheduler->CanChatOnline())
			{
				ChatItem->FromName = FText::FromString(*FoundFriend->GetName());
				ChatItem->SenderId = FoundFriend->GetUniqueID();
				ChatItem->bIsFromSelf = false;
				ChatItem->RecipientId = LoggedInUser;
				ChatItem->Message = FText::FromString(*ChatMessage->GetBody());
				ChatItem->MessageType = EChatMessageType::Whisper;
				ChatItem->MessageTime = FDateTime::Now();
				ChatItem->ExpireTime = FDateTime::Now() + FTimespan::FromSeconds(WhisperMessageLifetime);
				ChatItem->MessageRef = ChatMessage;
				WhisperMessagesCount++;
				AddMessage(ChatItem.ToSharedRef());

				// Inform listers that we have received a chat message
				OnChatMessageRecieved().Broadcast(ChatItem->MessageType, FoundFriend);
				NotificationService->SendMessageReceivedNotification(FoundFriend);
			}
		}
	}

	void AddMessage(TSharedRef< FFriendChatMessage > NewMessage)
	{
		//TSharedRef<FChatItemViewModel> NewMessage = FChatItemViewModelFactory::Create(ChatItem);
		if(ReceivedMessages.Add(NewMessage) > MessageStore)
		{
			bool bGlobalTimeFound = false;
			bool bGameTimeFound = false;
			bool bPartyTimeFound = false;
			bool bWhisperFound = false;
			bool bAdminFound = false;
			bool bLiveStreamFound = false;

			FDateTime CurrentTime = FDateTime::Now();
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
						case EChatMessageType::Game :
						{
							if(GameMessagesCount > GameMaxStore)
							{
								RemoveMessage(Message);
								Index--;
							}
							else
							{
								bGameTimeFound = true;
							}
						}
						break;
						case EChatMessageType::Party:
						{
							if (PartyMessagesCount > PartyMaxStore)
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
						case EChatMessageType::Admin :
						{
							if(AdminMessagesCount > AdminMaxStore)
							{
								RemoveMessage(Message);
								Index--;
							}
							else
							{
								bAdminFound = true;
							}
						}
						break;
						case EChatMessageType::LiveStreaming :
						{
							if(LiveStreamMessagesCount > LiveStreamMaxStore)
							{
								RemoveMessage(Message);
								Index--;
							}
							else
							{
								bLiveStreamFound = true;
							}
						}
						break;
					}
				}
				if (ReceivedMessages.Num() < MessageStore || (bPartyTimeFound && bGameTimeFound && bGlobalTimeFound && bWhisperFound && bAdminFound && bLiveStreamFound))
				{
					break;
				}
			}
		}
		OnChatMessageAdded().Broadcast(NewMessage);
	}

	void RemoveMessage(TSharedRef<FFriendChatMessage> Message)
	{
		switch(Message->MessageType)
		{
			case EChatMessageType::Global : GlobalMessagesCount--; break;
			case EChatMessageType::Game: GameMessagesCount--; break;
			case EChatMessageType::Party: PartyMessagesCount--; break;
			case EChatMessageType::Whisper : WhisperMessagesCount--; break;
			case EChatMessageType::Admin : AdminMessagesCount--; break;
			case EChatMessageType::LiveStreaming : LiveStreamMessagesCount--; break;
		}
		ReceivedMessages.Remove(Message);
	}

	bool GetGlobalChatRoomId(FString& OutGlobalRoomId) const
	{
		if (RoomJoins.Num() > 0)
		{
			OutGlobalRoomId = RoomJoins[0];
			return true;
		}
		return false;
	}

	void RegisterMessageSent()
	{
		ChatTicketCount--;
	}

	bool HandleTick(float Delay)
	{
		ChatTicketCount = FMath::Min(ChatTicketCount + 1, ChatTicketLimit);
		return true;
	}

	// Check if blocked
	bool IsPlayerMuted(const TSharedRef<const FUniqueNetId>& UserId)
	{
		return FriendsService->IsPlayerBlocked(UserId) || FriendsService->IsPlayerMuted(UserId);
	}

	FMessageServiceImpl(
		const TSharedRef<FOSSScheduler>& InOSSScheduler, 
		const TSharedRef<FFriendsService>& InFriendsService, 
		const TSharedRef<FGameAndPartyService>& InGamePartyService,
		const TSharedRef<FChatNotificationService>& InNotificationService)
		: ChatThrottleEnabled(false)
		, ChatTicketCount(0)
		, ChatTicketLimit(0)
		, bEnableEnterExitMessages(false)
		, LocalControllerIndex(0)
		, bJoinedGlobalChat(false)
		, OSSScheduler(InOSSScheduler)
		, FriendsService(InFriendsService)
		, GamePartyService(InGamePartyService)
		, NotificationService(InNotificationService)
	{
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

	// Handles to the above registered delegates
	FDelegateHandle OnChatRoomJoinPublicDelegateHandle;
	FDelegateHandle OnChatRoomExitDelegateHandle;
	FDelegateHandle OnChatRoomMemberJoinDelegateHandle;
	FDelegateHandle OnChatRoomMemberExitDelegateHandle;
	FDelegateHandle OnChatRoomMemberUpdateDelegateHandle;
	FDelegateHandle OnChatRoomMessageReceivedDelegateHandle;
	FDelegateHandle OnChatPrivateMessageReceivedDelegateHandle;

	// Outgoing events
	FOnChatMessageReceivedEvent MessageReceivedEvent;
	FOnFriendsSendNetworkMessageEvent FriendsSendNetworkMessageEvent;
	FOnChatMessageAddedEvent MessageAddedEvent;
	FOnChatPublicRoomJoinedEvent PublicRoomJoinedEvent;
	FOnChatPublicRoomExitedEvent PublicRoomExitedEvent;

	TSharedPtr<const FUniqueNetId> LoggedInUser;
	TArray<FString> RoomJoins;
	TArray<TSharedRef<FFriendChatMessage> > ReceivedMessages;

	// Message counts
	int32 GlobalMessagesCount;
	int32 WhisperMessagesCount;
	int32 GameMessagesCount;
	int32 PartyMessagesCount;
	int32 AdminMessagesCount;
	int32 LiveStreamMessagesCount;
	bool ChatThrottleEnabled;
	int32 ChatTicketCount;
	int32 ChatTicketLimit;

	// Delegate for which function we should use when we tick
	FTickerDelegate TickDelegate;
	// Handler for the tick function when active
	FDelegateHandle TickerHandle;


	bool bEnableEnterExitMessages;
	int32 LocalControllerIndex;
	bool bJoinedGlobalChat;

	// Services
	TSharedRef<FOSSScheduler> OSSScheduler;
	TSharedRef<FFriendsService> FriendsService;
	TSharedRef<FGameAndPartyService> GamePartyService;
	TSharedRef<FChatNotificationService> NotificationService;
	friend FMessageServiceFactory;
};

TSharedRef< FMessageService > FMessageServiceFactory::Create(
	const TSharedRef<FOSSScheduler>& OSSScheduler,
	const TSharedRef<class FFriendsService>& FriendsService,
	const TSharedRef<FGameAndPartyService>& GamePartyService,
	const TSharedRef<class FChatNotificationService>& NotificationService)
{
	TSharedRef< FMessageServiceImpl > MessageService(new FMessageServiceImpl(OSSScheduler, FriendsService, GamePartyService, NotificationService));
	return MessageService;
}

#undef LOCTEXT_NAMESPACE