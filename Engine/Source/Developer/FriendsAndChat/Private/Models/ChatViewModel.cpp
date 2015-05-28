// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ChatItemViewModel.h"
#include "ChatViewModel.h"
#include "FriendViewModel.h"
#include "OnlineChatInterface.h"

class FChatViewModelImpl
	: public FChatViewModel
{
public:
	virtual TArray< TSharedRef<FChatItemViewModel > >& GetMessages() override
	{
		return FilteredMessages;
	}

	virtual FReply HandleSelectionChanged(TSharedRef<FChatItemViewModel> ItemSelected) override
	{
		if(ItemSelected->GetMessageType() == EChatMessageType::Whisper)
		{
			if(ItemSelected->GetSenderID().IsValid())
			{
				TSharedPtr< IFriendItem > ExistingFriend = FFriendsAndChatManager::Get()->FindUser(*ItemSelected->GetSenderID());
				if(ExistingFriend.IsValid() && ExistingFriend->GetInviteStatus() == EInviteStatus::Accepted)
				{
					SetWhisperChannel(GetRecentFriend(ItemSelected->GetSenderName(), ItemSelected->GetSenderID()));
				}
			}
		}
		else
		{
			SetChatChannel(ItemSelected->GetMessageType());
		}

		return FReply::Handled();
	}

	virtual FText GetViewGroupText() const override
	{
		return EChatMessageType::ToText(SelectedViewChannel);
	}

	virtual FText GetChatGroupText() const override
	{
		return SelectedFriend.IsValid() ? SelectedFriend->DisplayName : EChatMessageType::ToText(SelectedChatChannel);
	}

	virtual void EnumerateFriendOptions(TArray<EFriendActionType::Type>& OUTActionList) override
	{
		// Enumerate actions
		if(SelectedFriend.IsValid() && SelectedFriend->UserID.IsValid())
		{
			if(SelectedFriend->ViewModel.IsValid())
			{
				SelectedFriend->ViewModel->EnumerateActions(OUTActionList, true);
			}
			else
			{
				OUTActionList.Add(EFriendActionType::SendFriendRequest);
			}
		}
		else
		{
			OUTActionList.Add(EFriendActionType::SendFriendRequest);
		}
	}

	virtual void PerformFriendAction(EFriendActionType::Type ActionType) override
	{
		if(SelectedFriend.IsValid())
		{
			if(SelectedFriend->ViewModel.IsValid())
			{
				SelectedFriend->ViewModel->PerformAction(ActionType);
			}
			else if(ActionType == EFriendActionType::SendFriendRequest)
			{
				FFriendsAndChatManager::Get()->RequestFriend(SelectedFriend->DisplayName);
				CancelAction();
			}
			else if(ActionType == EFriendActionType::InviteToGame)
			{
				if(SelectedFriend->UserID.IsValid())
				{
					FFriendsAndChatManager::Get()->SendGameInvite(*SelectedFriend->UserID.Get());
				}
				else if(SelectedFriend->SelectedMessage.IsValid() && SelectedFriend->SelectedMessage->GetSenderID().IsValid())
				{
					FFriendsAndChatManager::Get()->SendGameInvite(*SelectedFriend->SelectedMessage->GetSenderID().Get());
				}
			}
			else if (ActionType == EFriendActionType::Whisper)
			{
				TSharedPtr<IFriendItem> FriendItem = FFriendsAndChatManager::Get()->FindUser(*SelectedFriend->UserID);
				if (FriendItem.IsValid())
				{
					FFriendsAndChatManager::Get()->SetChatFriend(FriendItem);					
				}				
				CancelAction();
			}
			else if (ActionType == EFriendActionType::AcceptFriendRequest)
			{
				TSharedPtr<IFriendItem> FriendItem = FFriendsAndChatManager::Get()->FindUser(*SelectedFriend->UserID);
				if (FriendItem.IsValid())
				{
					FFriendsAndChatManager::Get()->AcceptFriend(FriendItem);
				}
				CancelAction();
			}
			else if (ActionType == EFriendActionType::IgnoreFriendRequest)
			{
				TSharedPtr<IFriendItem> FriendItem = FFriendsAndChatManager::Get()->FindUser(*SelectedFriend->UserID);
				if (FriendItem.IsValid())
				{
					FFriendsAndChatManager::Get()->DeleteFriend(FriendItem, EFriendActionType::ToText(EFriendActionType::IgnoreFriendRequest).ToString());
				}
				CancelAction();
			}
			else if (ActionType == EFriendActionType::CancelFriendRequest)
			{
				TSharedPtr<IFriendItem> FriendItem = FFriendsAndChatManager::Get()->FindUser(*SelectedFriend->UserID);
				if (FriendItem.IsValid())
				{
					FFriendsAndChatManager::Get()->DeleteFriend(FriendItem, EFriendActionType::ToText(EFriendActionType::CancelFriendRequest).ToString());
				}
				CancelAction();
			}
		}
	}

	virtual void CancelAction() 
	{
		bHasActionPending = false;
		SelectedFriend.Reset();
	}

	virtual void LockChatChannel(bool bLocked) override
	{
		bLockedChannel = bLocked;
		RefreshMessages();
	}

	virtual bool IsChatChannelLocked() const override
	{
		return bLockedChannel;
	}
	
	virtual void SetChatChannel(const EChatMessageType::Type NewOption) override
	{
		if (!bLockedChannel)
		{
			bHasActionPending = false;
			SelectedChatChannel = NewOption;
			if (NewOption == EChatMessageType::Global && !bIsDisplayingGlobalChat)
			{
				SetDisplayGlobalChat(true);
			}
			SelectedFriend.Reset();
		}
	}

	virtual void SetWhisperChannel(const TSharedPtr<FSelectedFriend> InFriend) override
	{
		if (!bLockedChannel)
		{
			SelectedChatChannel = EChatMessageType::Whisper;
			SelectedFriend = InFriend;
			if (SelectedFriend->UserID.IsValid())
			{
				SelectedFriend->ViewModel = FFriendsAndChatManager::Get()->GetFriendViewModel(*SelectedFriend->UserID.Get());
			}
			SelectedFriend->MessageType = EChatMessageType::Whisper;
			bHasActionPending = false;
		}
	}

	virtual void SetViewChannel(const EChatMessageType::Type NewOption) override
	{
		if (!bLockedChannel)
		{
		    SelectedViewChannel = NewOption;
		    SelectedChatChannel = NewOption;
			RefreshMessages();
		    bHasActionPending = false;
	    }
	}

	virtual const EChatMessageType::Type GetChatChannel() const override
	{
		return SelectedChatChannel;
	}

	virtual bool IsChatChannelValid() const override
	{
		return SelectedChatChannel != EChatMessageType::Global || bEnableGlobalChat;
	}

	virtual bool IsChatConnected() const override
	{
		// Are we online
		bool bConnected = FFriendsAndChatManager::Get()->GetOnlineStatus() != EOnlinePresenceState::Offline;

		// Is our friend online
		if (SelectedChatChannel == EChatMessageType::Whisper)
		{ 
			if(SelectedFriend.IsValid() &&
			SelectedFriend->ViewModel.IsValid())
			{
				bConnected &= SelectedFriend->ViewModel->IsOnline();
			}
			else
			{
				bConnected = false;
			}
		}

		return bConnected;
	}

	virtual FText GetChatDisconnectText() const override
	{
		if (FFriendsAndChatManager::Get()->GetOnlineStatus() == EOnlinePresenceState::Offline)
		{
			return NSLOCTEXT("ChatViewModel", "LostConnection", "Unable to chat while offline.");
		}
		if (SelectedChatChannel == EChatMessageType::Whisper )
		{
			if( SelectedFriend.IsValid() && 
			SelectedFriend->ViewModel.IsValid() &&
			!SelectedFriend->ViewModel->IsOnline())
			{
				return FText::Format(NSLOCTEXT("ChatViewModel", "FriendOffline", "{0} is now offline."), SelectedFriend->DisplayName);
			}
			else if(SelectedFriend.IsValid())
			{
				return FText::Format(NSLOCTEXT("ChatViewModel", "FriendUnavailable", "Unable to send whisper to {0}."), SelectedFriend->DisplayName);
			}
			else
			{
				return NSLOCTEXT("ChatViewModel", "FriendUnavailable", "Unable to send whisper.");
			}
		}
		return FText::GetEmpty();
	}

	virtual void SetChannelUserClicked(const TSharedRef<FChatItemViewModel> ChatItemSelected) override
	{
		if(ChatItemSelected->IsFromSelf() && ChatItemSelected->GetMessageType() == EChatMessageType::Global)
		{
			SetChatChannel(EChatMessageType::Global);
		}
		else
		{
			bool bFoundUser = false;
			TSharedPtr< IFriendItem > ExistingFriend = NULL;

			const TSharedPtr<FUniqueNetId> ChatID = ChatItemSelected->IsFromSelf() ? ChatItemSelected->GetRecipientID() : ChatItemSelected->GetSenderID();
			if(ChatID.IsValid())
			{
				ExistingFriend = FFriendsAndChatManager::Get()->FindUser(*ChatID.Get());
			}

			if(ExistingFriend.IsValid() && ExistingFriend->GetInviteStatus() == EInviteStatus::Accepted && !bLockedChannel)
			{
				bFoundUser = true;
				SetWhisperChannel(GetRecentFriend(FText::FromString(ExistingFriend->GetName()), ExistingFriend->GetUniqueID()));
			}			
			if (ExistingFriend.IsValid() && ExistingFriend->GetInviteStatus() == EInviteStatus::Accepted && !ExistingFriend->IsOnline() )
			{
				/* Its our friend but they are offline */
				bFoundUser = true;
			}
			if (ExistingFriend.IsValid() && ExistingFriend->GetInviteStatus() == EInviteStatus::Accepted && ExistingFriend->IsOnline() && !bIsDisplayingGlobalChat && bLockedChannel && !bAllowJoinGame)
			{
				/* Its our friend they are online but this is already a whisper chat window with just them and we cant invite them to join our game */
				bFoundUser = true;
			}

			if(!bFoundUser)
			{
				SetChatChannel(ChatItemSelected->GetMessageType());
				SelectedFriend = MakeShareable(new FSelectedFriend());
				SelectedFriend->DisplayName = ChatItemSelected->GetSenderName();
				SelectedFriend->MessageType = ChatItemSelected->GetMessageType();
				SelectedFriend->ViewModel = nullptr;
				SelectedFriend->SelectedMessage = ChatItemSelected;
				if (ExistingFriend.IsValid())
				{
					SelectedFriend->UserID = ExistingFriend->GetUniqueID();
				}
				bHasActionPending = true;
				bAllowJoinGame = FFriendsAndChatManager::Get()->IsInJoinableGameSession();
			}
		}
	}

	virtual bool SendMessage(const FText NewMessage) override
	{
		bool bSuccess = false;
		if(!NewMessage.IsEmpty())
		{
			switch(SelectedChatChannel)
			{
				case EChatMessageType::Whisper:
				{
					if (SelectedFriend.IsValid() && SelectedFriend->UserID.IsValid())
					{
						if (MessageManager.Pin()->SendPrivateMessage(SelectedFriend->UserID, NewMessage))
						{
							bSuccess = true;
						}

						FFriendsAndChatManager::Get()->GetAnalytics().RecordPrivateChat(SelectedFriend->UserID->ToString());
					}
				}
				break;
				case EChatMessageType::Global:
				{
					if (IsDisplayingGlobalChat())
					{
						//@todo samz - send message to specific room (empty room name will send to all rooms)
						bSuccess = MessageManager.Pin()->SendRoomMessage(FString(), NewMessage.ToString());

						FFriendsAndChatManager::Get()->GetAnalytics().RecordChannelChat(TEXT("Global"));
					}	
				}
				break;
			}
		}
		return bSuccess;
	}

	virtual EChatMessageType::Type GetChatChannelType() const
	{
		return SelectedChatChannel;
	}
	
	virtual const TArray<TSharedPtr<FSelectedFriend> >& GetRecentOptions() const override
	{
		return RecentPlayerList;
	}

	virtual EVisibility GetFriendRequestVisibility() const override
	{
		if (SelectedFriend.IsValid() && SelectedFriend->UserID.IsValid())
		{
			TSharedPtr<IFriendItem> FriendItem = FFriendsAndChatManager::Get()->FindUser(*SelectedFriend->UserID);
			if (FriendItem.IsValid())
			{
				return EVisibility::Collapsed;
			}
		}
		return EVisibility::Visible;
	}

	virtual EVisibility GetAcceptFriendRequestVisibility() const override
	{
		if (SelectedFriend.IsValid() && SelectedFriend->UserID.IsValid())
		{
			TSharedPtr<IFriendItem> FriendItem = FFriendsAndChatManager::Get()->FindUser(*SelectedFriend->UserID);
			if (FriendItem.IsValid() && 
				FriendItem->GetInviteStatus() == EInviteStatus::PendingInbound && 
				(!FriendItem->IsPendingAccepted() || !FriendItem->IsPendingDelete()))
			{
				return EVisibility::Visible;
			}
		}

		return EVisibility::Collapsed;
	}

	virtual EVisibility GetIgnoreFriendRequestVisibility() const override
	{
		if (SelectedFriend.IsValid() && SelectedFriend->UserID.IsValid())
		{
			TSharedPtr<IFriendItem> FriendItem = FFriendsAndChatManager::Get()->FindUser(*SelectedFriend->UserID);
			if (FriendItem.IsValid() &&
				FriendItem->GetInviteStatus() == EInviteStatus::PendingInbound &&
				(!FriendItem->IsPendingAccepted() || !FriendItem->IsPendingDelete()))
			{
				return EVisibility::Visible;
			}
		}

		return EVisibility::Collapsed;
	}

	virtual EVisibility GetCancelFriendRequestVisibility() const override
	{
		if (SelectedFriend.IsValid() && SelectedFriend->UserID.IsValid())
		{
			TSharedPtr<IFriendItem> FriendItem = FFriendsAndChatManager::Get()->FindUser(*SelectedFriend->UserID);
			if (FriendItem.IsValid() &&
				FriendItem->GetInviteStatus() == EInviteStatus::PendingOutbound &&
				(!FriendItem->IsPendingAccepted() || !FriendItem->IsPendingDelete()))
			{
				return EVisibility::Visible;
			}
		}

		return EVisibility::Collapsed;
	}

	virtual EVisibility GetInviteToGameVisibility() const override
	{
		return bAllowJoinGame ? EVisibility::Visible : EVisibility::Collapsed;
	}

	virtual EVisibility GetOpenWhisperVisibility() const override
	{
		if (bIsDisplayingGlobalChat && SelectedFriend.IsValid() && SelectedFriend->UserID.IsValid())
		{
			TSharedPtr<IFriendItem> FriendItem = FFriendsAndChatManager::Get()->FindUser(*SelectedFriend->UserID);			
			if (FriendItem.IsValid() && FriendItem->GetInviteStatus() == EInviteStatus::Accepted && FriendItem->IsOnline())
			{
				return EVisibility::Visible;
			}
		}
		return EVisibility::Collapsed;
	}

	virtual bool IsDisplayingGlobalChat() const override
	{
		return bEnableGlobalChat && bIsDisplayingGlobalChat;
	}

	virtual bool IsGlobalChatEnabled() const override
	{
		return bEnableGlobalChat;
	}

	virtual bool HasValidSelectedFriend() const override
	{
		return SelectedFriend.IsValid();
	}

	virtual bool HasFriendChatAction() const override
	{
		if (SelectedFriend.IsValid() && 
			SelectedFriend->ViewModel.IsValid())
		{
			return SelectedFriend->ViewModel->HasChatAction();
		}
		return false;
	}

	virtual bool HasActionPending() const override
	{
		return bHasActionPending;
	}

	virtual void SetDisplayGlobalChat(bool bAllow) override
	{
		bIsDisplayingGlobalChat = bAllow;
		FFriendsAndChatManager::Get()->GetAnalytics().RecordToggleChat(TEXT("Global"), bIsDisplayingGlobalChat, TEXT("Social.Chat.Toggle"));
		RefreshMessages();
	}

	virtual void EnableGlobalChat(bool bEnable) override
	{
		bEnableGlobalChat = bEnable;
		RefreshMessages();
	}

	virtual void SetInGame(bool bInGameSetting) override
	{
		if(bInGame != bInGameSetting)
		{
			bInGame = bInGameSetting;
			SetViewChannel(bInGame ? EChatMessageType::Party : EChatMessageType::Global);
		}
	}

	DECLARE_DERIVED_EVENT(FChatViewModelImpl , FChatViewModel::FChatListUpdated, FChatListUpdated);
	virtual FChatListUpdated& OnChatListUpdated() override
	{
		return ChatListUpdatedEvent;
	}

private:
	void Initialize()
	{
		FFriendsAndChatManager::Get()->OnFriendsListUpdated().AddSP(this, &FChatViewModelImpl::UpdateSelectedFriendsViewModel);
		FFriendsAndChatManager::Get()->OnChatFriendSelected().AddSP(this, &FChatViewModelImpl::HandleChatFriendSelected);
		MessageManager.Pin()->OnChatMessageRecieved().AddSP(this, &FChatViewModelImpl::HandleMessageReceived);
		RefreshMessages();
	}

	void RefreshMessages()
	{
		const TArray<TSharedRef<FFriendChatMessage>>& Messages = MessageManager.Pin()->GetMessages();
		FilteredMessages.Empty();

		bool AddedItem = false;
		for (const auto& Message : Messages)
		{
			AddedItem |= FilterMessage(Message);
		}

		if (AddedItem)
		{
			OnChatListUpdated().Broadcast();
		}
	}

	bool FilterMessage(const TSharedRef<FFriendChatMessage>& Message)
	{
		bool Changed = false;

		if (!IsDisplayingGlobalChat() || bLockedChannel)
		{
			if (Message->MessageType != EChatMessageType::Global)
			{
				// If channel is locked only show chat from current friend
				if (bLockedChannel && SelectedFriend.IsValid() && !bIsDisplayingGlobalChat)
				{
					if ((Message->bIsFromSelf && Message->RecipientId == SelectedFriend->UserID) || Message->SenderId == SelectedFriend->UserID)
					{
						AggregateMessage(Message);
						Changed = true;
					}
				}

				// If its not locked show all chat
				if (!bLockedChannel)
				{
					AggregateMessage(Message);
					Changed = true;
				}
			}
			else
			{
				// If channel is locked only show global chat
				if (bLockedChannel && bIsDisplayingGlobalChat)
				{
					AggregateMessage(Message);
					Changed = true;
				}
			}
		}
		else
		{
			AggregateMessage(Message);
			Changed = true;
		}

		return Changed;
	}

	void AggregateMessage(const TSharedRef<FFriendChatMessage>& Message)
	{
		TSharedPtr<FChatItemViewModel> PreviousMessage;
		if (FilteredMessages.Num() > 0)
		{
			PreviousMessage = FilteredMessages.Last();
		}

		if (PreviousMessage.IsValid() && 
			PreviousMessage->GetSenderID().IsValid() &&
			Message->SenderId.IsValid() &&
			*PreviousMessage->GetSenderID() == *Message->SenderId &&
			PreviousMessage->GetMessageType() == Message->MessageType &&
			(Message->MessageTime - PreviousMessage->GetMessageTime()).GetDuration() <= FTimespan::FromMinutes(1.0))
		{
			PreviousMessage->AddMessage(Message);
		}
		else
		{
			FilteredMessages.Add(FChatItemViewModelFactory::Create(Message));
		}
	}

	TSharedPtr<FSelectedFriend> FindFriend(TSharedPtr<FUniqueNetId> UniqueID)
	{
		// ToDo - Make this nicer NickD
		for( const auto& ExistingFriend : RecentPlayerList)
		{
			if(ExistingFriend->UserID == UniqueID)
			{
				return	ExistingFriend;
			}
		}
		return nullptr;
	}

	void HandleMessageReceived(const TSharedRef<FFriendChatMessage> NewMessage)
	{
		if(IsDisplayingGlobalChat() || NewMessage->MessageType != EChatMessageType::Global)
		{
			if (FilterMessage(NewMessage))
			{
				OnChatListUpdated().Broadcast();
			}
		}
	}

	void HandleChatFriendSelected(TSharedPtr<IFriendItem> ChatFriend)
	{
		if(ChatFriend.IsValid())
		{
			SetWhisperChannel(GetRecentFriend(FText::FromString(ChatFriend->GetName()), ChatFriend->GetUniqueID()));
		}
		else if (bIsDisplayingGlobalChat)
		{
			SetChatChannel(EChatMessageType::Global);
		}
	}

	TSharedRef<FSelectedFriend> GetRecentFriend(const FText Username, TSharedPtr<FUniqueNetId> UniqueID)
	{
		TSharedPtr<FSelectedFriend> NewFriend = FindFriend(UniqueID);
		if (!NewFriend.IsValid())
		{
			NewFriend = MakeShareable(new FSelectedFriend());
			NewFriend->DisplayName = Username;
			NewFriend->UserID = UniqueID;
			RecentPlayerList.AddUnique(NewFriend);
		}
		return NewFriend.ToSharedRef();
	}

	void UpdateSelectedFriendsViewModel()
	{
		if( SelectedFriend.IsValid())
		{
			if(SelectedFriend->UserID.IsValid())
			{
				SelectedFriend->ViewModel = FFriendsAndChatManager::Get()->GetFriendViewModel(*SelectedFriend->UserID.Get());
				bHasActionPending = false;
			}
			else if(SelectedFriend->SelectedMessage.IsValid() && SelectedFriend->SelectedMessage->GetSenderID().IsValid())
			{
				SelectedFriend->ViewModel = FFriendsAndChatManager::Get()->GetFriendViewModel(*SelectedFriend->SelectedMessage->GetSenderID().Get());
				bHasActionPending = false;
			}
		}
	}

	FChatViewModelImpl(const TSharedRef<FFriendsMessageManager>& InMessageManager)
		: SelectedViewChannel(EChatMessageType::Global)
		, SelectedChatChannel(EChatMessageType::Global)
		, MessageManager(InMessageManager)
		, bInGame(false)
		, bIsDisplayingGlobalChat(false)
		, bEnableGlobalChat(true)
		, bHasActionPending(false)
		, bAllowJoinGame(false)
		, bLockedChannel(false)
	{
	}

private:
	EChatMessageType::Type SelectedViewChannel;
	EChatMessageType::Type SelectedChatChannel;
	TWeakPtr<FFriendsMessageManager> MessageManager;
	bool bInGame;
	/** Whether global chat is currently switched on and displayed  */
	bool bIsDisplayingGlobalChat;
	/** Whether global chat mode is enabled/disabled - if disabled it can't be selected and isn't an option in the UI */
	bool bEnableGlobalChat;
	bool bHasActionPending;
	bool bAllowJoinGame;
	bool bLockedChannel;

	TArray<TSharedRef<FChatItemViewModel> > FilteredMessages;
	TArray<TSharedPtr<FSelectedFriend> > RecentPlayerList;
	TSharedPtr<FSelectedFriend> SelectedFriend;
	FChatListUpdated ChatListUpdatedEvent;

private:
	friend FChatViewModelFactory;
};

TSharedRef< FChatViewModel > FChatViewModelFactory::Create(
	const TSharedRef<FFriendsMessageManager>& MessageManager
	)
{
	TSharedRef< FChatViewModelImpl > ViewModel(new FChatViewModelImpl(MessageManager));
	ViewModel->Initialize();
	return ViewModel;
}