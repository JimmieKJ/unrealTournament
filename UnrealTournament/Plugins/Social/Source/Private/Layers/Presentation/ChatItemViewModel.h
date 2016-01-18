// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FFriendChatMessage;
class FChatViewModel;

class FChatItemViewModel
	: public TSharedFromThis<FChatItemViewModel>
{
public:
	virtual ~FChatItemViewModel() {}

	virtual FText GetMessage() const = 0;
	virtual const EChatMessageType::Type GetMessageType() const =0;
	virtual FDateTime GetMessageTime() const = 0;

	virtual void AddMessage(const TSharedRef<FFriendChatMessage>& ChatMessage) = 0;

	virtual FText GetSenderName() const = 0;
	virtual const TSharedPtr<const FUniqueNetId> GetSenderID() const = 0;

	virtual FText GetRecipientName() const = 0;
	virtual const TSharedPtr<const FUniqueNetId> GetRecipientID() const = 0;

	virtual const bool IsFromSelf() const = 0;
};

/**
 * Creates the implementation for an ChatItemViewModel.
 *
 * @return the newly created ChatItemViewModel implementation.
 */
FACTORY(TSharedRef< FChatItemViewModel >, FChatItemViewModel,
	const TSharedRef<FFriendChatMessage>& FFriendChatMessage);