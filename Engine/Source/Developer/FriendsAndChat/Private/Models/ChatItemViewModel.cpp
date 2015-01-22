// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ChatItemViewModel.h"
#include "ChatViewModel.h"

class FChatItemViewModelImpl
	: public FChatItemViewModel
{
public:

	virtual FText GetMessage() override
	{
		return ChatMessage->Message;
	}

	virtual const EChatMessageType::Type GetMessageType() const override
	{
		return ChatMessage->MessageType;
	}

	virtual FText GetMessageTypeText() override
	{
		return EChatMessageType::ToText(ChatMessage->MessageType);
	}

	virtual const FText GetFriendNameDisplayText() const override
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Username"), ChatMessage->FromName);
		const FText DisplayName = FText::Format(NSLOCTEXT("FChatItemViewModel", "DisplayName", "{Username}: "), Args);
		return DisplayName;
	}

	virtual const FText GetFriendName() const override
	{
		return ChatMessage->FromName;
	}

	virtual const TSharedPtr<FUniqueNetId> GetSenderID() const override
	{
		return ChatMessage->SenderId;
	}

	virtual FText GetMessageTime() override
	{
		return ChatMessage->MessageTimeText;
	}

	virtual const FDateTime GetExpireTime() override
	{
		return ChatMessage->ExpireTime;
	}

	const bool IsFromSelf() const override
	{
		return ChatMessage->bIsFromSelf;
	}

private:

	FChatItemViewModelImpl(TSharedRef<FFriendChatMessage> ChatMessage)
	: ChatMessage(ChatMessage)
	{
	}

private:
	TSharedRef<FFriendChatMessage> ChatMessage;

private:
	friend FChatItemViewModelFactory;
};

TSharedRef< FChatItemViewModel > FChatItemViewModelFactory::Create(const TSharedRef<FFriendChatMessage>& ChatMessage)
{
	TSharedRef< FChatItemViewModelImpl > ViewModel(new FChatItemViewModelImpl(ChatMessage));
	return ViewModel;
}
