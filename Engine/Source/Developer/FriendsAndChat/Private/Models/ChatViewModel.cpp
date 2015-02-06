// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ChatItemViewModel.h"
#include "ChatViewModel.h"
#include "OnlineChatInterface.h"

class FChatViewModelImpl
	: public FChatViewModel
{
public:
	virtual TArray< TSharedRef<FChatItemViewModel > >& GetFilteredChatList() override
	{
		return FilteredChatLists;
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
					SetWhisperChannel(GetRecentFriend(ItemSelected->GetFriendName(), ItemSelected->GetSenderID()));
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
		return SelectedFriend.IsValid() ? SelectedFriend->FriendName : EChatMessageType::ToText(SelectedChatChannel);
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
				FFriendsAndChatManager::Get()->RequestFriend(SelectedFriend->FriendName);
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
		}
	}

	virtual void CancelAction() 
	{
		bHasActionPending = false;
		SelectedFriend.Reset();
	}

	virtual void SetChatChannel(const EChatMessageType::Type NewOption) override
	{
		bHasActionPending = false;
		SelectedChatChannel = NewOption;
		if(NewOption == EChatMessageType::Global && !bAllowGlobalChat)
		{
			SetAllowGlobalChat(true);
		}

		SelectedFriend.Reset();
	}

	virtual void SetWhisperChannel(const TSharedPtr<FSelectedFriend> InFriend) override
	{
		SelectedChatChannel = EChatMessageType::Whisper;
		SelectedFriend = InFriend;
		if(SelectedFriend->UserID.IsValid())
		{
			SelectedFriend->ViewModel = FFriendsAndChatManager::Get()->GetFriendViewModel(*SelectedFriend->UserID.Get());
		}
		SelectedFriend->MessageType = EChatMessageType::Whisper;
		bHasActionPending = false;
	}

	virtual void SetViewChannel(const EChatMessageType::Type NewOption) override
	{
		SelectedViewChannel = NewOption;
		SelectedChatChannel = NewOption;
		FilterChatList();
		bHasActionPending = false;
	}

	virtual const EChatMessageType::Type GetChatChannel() const override
	{
		return SelectedChatChannel;
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

			if(ChatItemSelected->GetSenderID().IsValid())
			{
				ExistingFriend = FFriendsAndChatManager::Get()->FindUser(*ChatItemSelected->GetSenderID().Get());
			}

			if(ExistingFriend.IsValid() && ExistingFriend->GetInviteStatus() == EInviteStatus::Accepted)
			{
				bFoundUser = true;
				SetWhisperChannel(GetRecentFriend(FText::FromString(ExistingFriend->GetName()), ExistingFriend->GetUniqueID()));
			}

			if(!bFoundUser)
			{
				SetChatChannel(ChatItemSelected->GetMessageType());
				SelectedFriend = MakeShareable(new FSelectedFriend());
				SelectedFriend->FriendName = ChatItemSelected->GetFriendName();
				SelectedFriend->MessageType = ChatItemSelected->GetMessageType();
				SelectedFriend->ViewModel = nullptr;
				SelectedFriend->SelectedMessage = ChatItemSelected;
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
						if (MessageManager.Pin()->SendPrivateMessage(SelectedFriend->UserID, SelectedFriend->FriendName, NewMessage))
						{
							bSuccess = true;
						}

						FFriendsAndChatManager::Get()->GetAnalytics().RecordPrivateChat(SelectedFriend->UserID->ToString());
					}
				}
				break;
				case EChatMessageType::Global:
				{
					if (IsGlobalChatEnabled())
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

	virtual EVisibility GetInviteToGameVisibility() const override
	{
		return bAllowJoinGame ? EVisibility::Visible : EVisibility::Collapsed;
	}

	virtual bool IsGlobalChatEnabled() const override
	{
		return bAllowGlobalChat;
	}

	virtual bool HasValidSelectedFriend() const override
	{
		return SelectedFriend.IsValid();
	}

	virtual bool HasFriendChatAction() const override
	{
		if(SelectedFriend.IsValid() && SelectedFriend->ViewModel.IsValid())
		{
			return SelectedFriend->ViewModel->HasChatAction();
		}
		return false;
	}

	virtual bool HasActionPending() const override
	{
		return bHasActionPending;
	}

	virtual void SetAllowGlobalChat(bool bAllow) override
	{
		bAllowGlobalChat = bAllow;
		FilterChatList();
	}

	virtual void SetInGame(bool bInGameSetting) override
	{
		if(bInGame != bInGameSetting)
		{
			SetAllowGlobalChat(bInGame);
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
		FilterChatList();
	}

	void FilterChatList()
	{
		ChatLists = MessageManager.Pin()->GetMessageList();

		bool bAddedItem = true;
		if(!IsGlobalChatEnabled())
		{
			FilteredChatLists.Empty();
			bAddedItem = false;
			for (const auto& ChatItem : ChatLists)
			{
				if(ChatItem->GetMessageType() != EChatMessageType::Global)
				{
					FilteredChatLists.Add(ChatItem);
					bAddedItem = true;
				}
			}
		}
		else
		{
			FilteredChatLists = ChatLists;
		}

		if(bAddedItem)
		{
			OnChatListUpdated().Broadcast();
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

	void HandleMessageReceived(const TSharedRef<FChatItemViewModel> NewMessage)
	{
		if(IsGlobalChatEnabled() || NewMessage->GetMessageType() != EChatMessageType::Global)
		{
			FilterChatList();
		}
	}

	void HandleChatFriendSelected(TSharedPtr<IFriendItem> ChatFriend)
	{
		if(ChatFriend.IsValid())
		{
			SetWhisperChannel(GetRecentFriend(FText::FromString(ChatFriend->GetName()), ChatFriend->GetUniqueID()));
		}
	}

	TSharedRef<FSelectedFriend> GetRecentFriend(const FText Username, TSharedPtr<FUniqueNetId> UniqueID)
	{
		TSharedPtr<FSelectedFriend> NewFriend = FindFriend(UniqueID);
		if (!NewFriend.IsValid())
		{
			NewFriend = MakeShareable(new FSelectedFriend());
			NewFriend->FriendName = Username;
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

	FChatViewModelImpl(const TSharedRef<FFriendsMessageManager>& MessageManager)
		: SelectedViewChannel(EChatMessageType::Global)
		, SelectedChatChannel(EChatMessageType::Global)
		, MessageManager(MessageManager)
		, bInGame(false)
		, bAllowGlobalChat(true)
		, bHasActionPending(false)
		, bAllowJoinGame(false)
	{
	}

private:
	EChatMessageType::Type SelectedViewChannel;
	EChatMessageType::Type SelectedChatChannel;
	TWeakPtr<FFriendsMessageManager> MessageManager;
	bool bInGame;
	bool bAllowGlobalChat;
	bool bHasActionPending;
	bool bAllowJoinGame;

	TArray<TSharedRef<FChatItemViewModel> > ChatLists;
	TArray<TSharedRef<FChatItemViewModel> > FilteredChatLists;
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