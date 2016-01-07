// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "ChatItemViewModel.h"
#include "ChatViewModel.h"

class FChatItemViewModelImpl
	: public FChatItemViewModel
{
public:

	virtual FText GetMessage() const override
	{
		return ComposedMessage;
	}

	virtual const EChatMessageType::Type GetMessageType() const override
	{
		return ChatMessages[0]->MessageType;
	}

	virtual FDateTime GetMessageTime() const override
	{
		return ChatMessages.Last()->MessageTime;
	}

	virtual void AddMessage(const TSharedRef<FFriendChatMessage>& ChatMessage) override
	{
		ChatMessages.Add(ChatMessage);

		FTextBuilder MessageBuilder;
		for (int32 Index = 0; Index < ChatMessages.Num(); Index++)
		{
			if (Index > 0)
			{
				static FString MessageBreak(TEXT("<MessageBreak></>"));
				MessageBuilder.AppendLine(MessageBreak);
			}

			MessageBuilder.AppendLine(ChatMessages[Index]->Message);
		}

		ComposedMessage = MessageBuilder.ToText();
	}

	virtual FText GetSenderName() const override
	{
		return ChatMessages[0]->FromName;
	}

	virtual FText GetRecipientName() const override
	{
		return ChatMessages[0]->ToName;
	}

	virtual const TSharedPtr<const FUniqueNetId> GetSenderID() const override
	{
		return ChatMessages[0]->SenderId;
	}

	virtual const TSharedPtr<const FUniqueNetId> GetRecipientID() const override
	{
		return ChatMessages[0]->RecipientId;
	}

	const bool IsFromSelf() const override
	{
		return ChatMessages[0]->bIsFromSelf;
	}

private:

	FChatItemViewModelImpl()
	{}

private:

	TArray<TSharedRef<FFriendChatMessage>> ChatMessages;

	FText ComposedMessage;

	friend FChatItemViewModelFactory;
};

TSharedRef< FChatItemViewModel > FChatItemViewModelFactory::Create(
	const TSharedRef<FFriendChatMessage>& ChatMessage)
{
	TSharedRef< FChatItemViewModelImpl > ViewModel = MakeShareable(new FChatItemViewModelImpl());

	ViewModel->AddMessage(ChatMessage);
	return ViewModel;
}
