// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatEntryViewModel.h"
#include "ChatViewModel.h"

#include "FriendsNavigationService.h"
#include "FriendsChatMarkupService.h"
#include "ChatDisplayService.h"
#include "GameAndPartyService.h"
#include "FriendsAndChatAnalytics.h"

class FChatEntryViewModelImpl
	: public FChatEntryViewModel
{
public:

	virtual void SetChatViewModel(TSharedRef<FChatViewModel> InActiveChatViewModel) override
	{
		ActiveChatViewModel = InActiveChatViewModel;
	}

	virtual FText GetChatDisconnectText() const override
	{
		if(MessageService->IsThrottled())
		{
			return NSLOCTEXT("ChatViewModel", "Throttled", "Throttled. Please wait...");
		}
		if (MessageService->GetOnlineStatus() == EOnlinePresenceState::Offline)
		{
			return NSLOCTEXT("ChatViewModel", "LostConnection", "Unable to chat while offline.");
		}
		if (ActiveChatViewModel.IsValid() && ActiveChatViewModel->GetDefaultChannelType() == EChatMessageType::Whisper)
		{
			if (ActiveChatViewModel->GetSelectedFriendViewModel().IsValid() && !ActiveChatViewModel->GetSelectedFriendViewModel()->IsOnline())
			{
				return FText::Format(NSLOCTEXT("ChatViewModel", "FriendOffline", "{0} is now offline. Enter / to pick a new channel"), FText::FromString(ActiveChatViewModel->GetSelectedFriendViewModel()->GetName()));
			}
			else if(ActiveChatViewModel->GetSelectedFriendViewModel().IsValid())
			{
				return FText::Format(NSLOCTEXT("ChatViewModel", "FriendUnavailable_WithDisplayName", "Unable to send whisper to {0}. Enter / to pick a new channel"), FText::FromString(ActiveChatViewModel->GetSelectedFriendViewModel()->GetName()));
			}
			else
			{
				return NSLOCTEXT("ChatViewModel", "FriendUnavailable", "Type \"/\" for options");
			}
		}
		return FText::GetEmpty();
	}

	virtual bool ShouldCaptureFocus() override
	{
		if(ActiveChatViewModel.IsValid())
		{
			return ActiveChatViewModel->ShouldCaptureFocus();
		}
		return false;
	}

	virtual EChatMessageType::Type GetMarkupChannel() const override
	{
		const EChatMessageType::Type MarkupChannel = MarkupService->GetMarkupChannel();
		if(MarkupChannel != EChatMessageType::Invalid)
		{
			return MarkupChannel;
		}
		else if(ActiveChatViewModel.IsValid())
		{
			return ActiveChatViewModel->GetDefaultChannelType();
		}
		return EChatMessageType::Invalid;
	}

	virtual FReply HandleChatKeyEntry(const FKeyEvent& KeyEvent) override
	{
		if (KeyEvent.GetKey() == EKeys::Escape)
		{
			ChatDisplayService->OnFocuseReleasedEvent().Broadcast();
		}
		ChatDisplayService->ChatEntered();
		return AllowMarkup() ? MarkupService->HandleChatKeyEntry(KeyEvent) : FReply::Unhandled();
	}

	virtual void ValidateChatInput(const FText Message, const FText PlainText) override
	{
		if (AllowMarkup())
		{
			MarkupService->SetInputText(Message.ToString(), PlainText.ToString());
			if (!Message.IsEmpty())
			{
				ChatDisplayService->ChatEntered();
			}
		}
	}

	virtual FText GetValidatedInput() override
	{
		return MarkupService->GetValidatedInput();
	}

	virtual bool SendMessage(const FText NewMessage, const FText PlainText) override
	{
		bool bSuccess = true;
		bool bMessageSent = false;
		if (!AllowMarkup() || !MarkupService->ValidateSlashMarkup(NewMessage.ToString(), PlainText.ToString()))
		{
			if(!PlainText.IsEmptyOrWhitespace())
			{
				if(!PlainText.IsEmpty() && ActiveChatViewModel.IsValid())
				{
					// Combine game and party settings
					EChatMessageType::Type MessageChannel = ActiveChatViewModel->GetOutgoingChatChannel();
					if (GamePartyService->ShouldCombineGameChatIntoPartyTab())
					{
						if (MessageChannel == EChatMessageType::Party && GamePartyService->ShouldUseGameChatForPartyOutput())
						{
							MessageChannel = EChatMessageType::Game;
						}
					}

					switch (MessageChannel)
					{
						case EChatMessageType::Whisper:
						{
							TSharedPtr<FFriendViewModel> SelectedFriend = ActiveChatViewModel->GetSelectedFriendViewModel();
							if (SelectedFriend.IsValid())
							{
								bSuccess = MessageService->SendPrivateMessage(SelectedFriend->GetFriendItem(), PlainText);
								bMessageSent = true;
							}
						}
						break;
						case EChatMessageType::Party:
						{
							FChatRoomId PartyChatRoomId = GamePartyService->GetPartyChatRoomId();
							if (GamePartyService->IsInPartyChat() && !PartyChatRoomId.IsEmpty())
							{
								//@todo will need to support multiple party channels eventually, hardcoded to first party for now
								bSuccess = MessageService->SendRoomMessage(PartyChatRoomId, NewMessage.ToString());
								bMessageSent = true;

								MessageService->GetAnalytics()->RecordChannelChat(TEXT("Party"));
							}
						}
						break;
						case EChatMessageType::Team:
						{
							FChatRoomId TeamChatRoomId = GamePartyService->GetTeamChatRoomId();
							if (GamePartyService->IsInTeamChat() && !TeamChatRoomId.IsEmpty())
							{
								//@todo will need to support multiple party channels eventually, hardcoded to first party for now
								bSuccess = MessageService->SendRoomMessage(TeamChatRoomId, NewMessage.ToString());
								bMessageSent = true;

								MessageService->GetAnalytics()->RecordChannelChat(TEXT("Team"));
							}
						}
						break;
						case EChatMessageType::Global:
						{
							//@todo samz - send message to specific room (empty room name will send to all rooms)
							bSuccess = MessageService->SendRoomMessage(FString(), PlainText.ToString());
							MessageService->GetAnalytics()->RecordChannelChat(TEXT("Global"));
							bMessageSent = true;
						}
						break;
						case EChatMessageType::Game:
						{
							MessageService->SendGameMessage(PlainText.ToString());
							MessageService->GetAnalytics()->RecordChannelChat(TEXT("Game"));
							bMessageSent = true;
							bSuccess = true;
						}
						break;
						case EChatMessageType::LiveStreaming:
						{
							ChatDisplayService->SendLiveStreamMessage(PlainText.ToString());
							MessageService->GetAnalytics()->RecordChannelChat(TEXT("LiveStreaming"));
							bMessageSent = true;
							bSuccess = true;
						}
						break;
					}
				}
			}
		}

		ChatDisplayService->MessageCommitted();
		MarkupService->CloseChatTips();

		return bSuccess;
	}

	virtual bool IsChatConnected() const override
	{
		if(MessageService->IsThrottled())
		{
			return false;
		}

		// Are we online
		bool bConnected = MessageService->GetOnlineStatus() != EOnlinePresenceState::Offline;

		// Is our friend online
		if (ActiveChatViewModel.IsValid() && ActiveChatViewModel->GetDefaultChannelType() == EChatMessageType::Whisper)
		{ 
			bConnected &= ActiveChatViewModel->GetSelectedFriendViewModel().IsValid() && ActiveChatViewModel->GetSelectedFriendViewModel()->IsOnline() && ActiveChatViewModel->GetSelectedFriendViewModel()->GetFriendItem()->GetInviteStatus() == EInviteStatus::Accepted;
		}
		return bConnected;
	}

	virtual TSharedPtr< FFriendViewModel> GetFriendViewModel(const FString InUserID, const FText Username) override
	{
		if(ActiveChatViewModel.IsValid())
		{
			return ActiveChatViewModel->GetFriendViewModel(InUserID, Username);
		}
		return NULL;
	}

	virtual void SetFocus() override
	{
		OnChatListSetFocus().Broadcast();
	}

	DECLARE_DERIVED_EVENT(FChatEntryViewModelImpl, FChatEntryViewModel::FChatTextValidatedEvent, FChatTextValidatedEvent);
	virtual FChatTextValidatedEvent& OnTextValidated() override
	{
		return ChatTextValidatedEvent;
	}

	DECLARE_DERIVED_EVENT(FChatEntryViewModelImpl, FChatEntryViewModel::FChatEntrySetFocusEvent, FChatEntrySetFocusEvent);
	virtual FChatEntrySetFocusEvent& OnChatListSetFocus() override
	{
		return ChatListSetFocusEvent;
	}

private:

	void HandleOnTextValidated()
	{
		OnTextValidated().Broadcast();
	}

	void HandleChatListFocused()
	{
		SetFocus();
	}

	void Initialize()
	{
		ChatDisplayService->OnChatListSetFocus().AddSP(this, &FChatEntryViewModelImpl::HandleChatListFocused);
		if(MarkupService.IsValid())
		{
			MarkupService->OnValidateInputReady().AddSP(this, &FChatEntryViewModelImpl::HandleOnTextValidated);
		}
	}

	bool AllowMarkup()
	{
		return MarkupService.IsValid();
	}

	FChatEntryViewModelImpl(
		const TSharedRef<FMessageService>& InMessageService,
		const TSharedRef<FChatDisplayService>& InChatDisplayService,
		const TSharedPtr<FFriendsChatMarkupService>& InMarkupService,
		const TSharedRef<FGameAndPartyService>& InGamePartyService)
		: MessageService(InMessageService)
		, ChatDisplayService(InChatDisplayService)
		, MarkupService(InMarkupService)
		, GamePartyService(InGamePartyService)
	{
	}

private:

	TSharedRef<FMessageService> MessageService;
	TSharedRef<FChatDisplayService> ChatDisplayService;
	TSharedPtr<FFriendsChatMarkupService> MarkupService;
	TSharedRef<FGameAndPartyService> GamePartyService;

	FChatEntrySetFocusEvent ChatListSetFocusEvent;
	FChatTextValidatedEvent ChatTextValidatedEvent;
	TSharedPtr<FChatViewModel> ActiveChatViewModel;

	friend FChatEntryViewModelFactory;
};

// Factory
TSharedRef<FChatEntryViewModel> FChatEntryViewModelFactory::Create(
	const TSharedRef<FMessageService>& MessageService,
	const TSharedRef<FChatDisplayService>& ChatDisplayService,
	const TSharedPtr<FFriendsChatMarkupService>& MarkupService,
	const TSharedRef<FGameAndPartyService>& GamePartyService)
{
	TSharedRef< FChatEntryViewModelImpl > ViewModel(new FChatEntryViewModelImpl(MessageService, ChatDisplayService, MarkupService, GamePartyService));
	ViewModel->Initialize();
	return ViewModel;
}

#undef LOCTEXT_NAMESPACE