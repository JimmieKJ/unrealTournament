// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatChromeTabViewModel.h"
#include "ChatViewModel.h"
#include "ChatDisplayService.h"

class FChatChromeTabViewModelImpl
	: public FChatChromeTabViewModel
{
public:

	FChatChromeTabViewModelImpl(const TSharedRef<FChatViewModel>& InChatViewModel)
		: ChatViewModel(InChatViewModel)
		, HasPendingMessage(false)
	{
	}

	virtual bool IsTabVisible() const override
	{
		if (!ChatViewModel.IsValid())
		{
			return false;
		}

		if (ChatViewModel->GetDefaultChannelType() == EChatMessageType::Party)
		{
			return ChatViewModel->IsInPartyChat() || ChatViewModel->IsInGameChat();
		}

		if (ChatViewModel->GetDefaultChannelType() == EChatMessageType::Game)
		{
			return ChatViewModel->IsInGameChat();
		}

		if (ChatViewModel->GetDefaultChannelType() == EChatMessageType::Whisper)
		{
			return (ChatViewModel->IsWhisperFriendSet() || ChatViewModel->HasDefaultChannelMessage() || ChatViewModel->IsOverrideDisplaySet());
		}

		if (ChatViewModel->GetDefaultChannelType() == EChatMessageType::LiveStreaming)
		{
			return ChatViewModel->IsLiveStreaming();
		}

		if (ChatViewModel->GetDefaultChannelType() == EChatMessageType::Team)
		{
			return ChatViewModel->IsInTeamChat();
		}

		if (ChatViewModel->GetDefaultChannelType() == EChatMessageType::Global)
		{
			return ChatViewModel->IsGlobalEnabled();
		}

		if (ChatViewModel->GetDefaultChannelType() == EChatMessageType::Combined)
		{
			return false;
		}

		return true;
	}

	virtual const FText GetTabText() const override
	{
		return ChatViewModel->GetChatGroupText(false);
	}

	virtual const FText GetOutgoingChannelText() const override
	{
		return ChatViewModel->GetChatGroupText(true);
	}

	virtual bool HasPendingMessages() const override
	{
		return HasPendingMessage;
	}

	virtual void SetPendingMessage() override
	{
		HasPendingMessage = true;
	}

	virtual void ClearPendingMessages() override
	{
		HasPendingMessage = false;
	}

	virtual const EChatMessageType::Type GetTabID() const override
	{
		return ChatViewModel->GetDefaultChannelType();
	}

	virtual const EChatMessageType::Type GetOutgoingChannel() const override
	{
		return ChatViewModel->GetOutgoingChatChannel();
	}

	virtual const FSlateBrush* GetTabImage() const override
	{
		return nullptr;
	}

	virtual const TSharedPtr<FChatViewModel> GetChatViewModel() const override
	{
		return ChatViewModel;
	}

	virtual TSharedRef<FChatChromeTabViewModel> Clone(TSharedRef<FChatDisplayService> ChatDisplayService) override
	{
		TSharedRef< FChatChromeTabViewModelImpl > ViewModel(new FChatChromeTabViewModelImpl(ChatViewModel->Clone(ChatDisplayService)));
		return ViewModel;
	}

	DECLARE_DERIVED_EVENT(FChatChromeTabViewModelImpl, FChatChromeTabViewModel::FChatTabVisibilityChangedEvent, FChatTabVisibilityChangedEvent)
	virtual FChatTabVisibilityChangedEvent& OnTabVisibilityChanged() override
	{
		return TabVisibilityChangedEvent;
	}

	TSharedPtr<FChatViewModel> ChatViewModel;

private:

	FChatTabVisibilityChangedEvent TabVisibilityChangedEvent;
	bool HasPendingMessage;

	friend FChatChromeTabViewModelFactory;
};

TSharedRef< FChatChromeTabViewModel > FChatChromeTabViewModelFactory::Create(const TSharedRef<FChatViewModel>& ChatViewModel)
{
	TSharedRef< FChatChromeTabViewModelImpl > ViewModel(new FChatChromeTabViewModelImpl(ChatViewModel));
	return ViewModel;
}