// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FriendViewModel.h"

class FChatEntryViewModel
	:public TSharedFromThis<FChatEntryViewModel>
{
public:

	virtual ~FChatEntryViewModel() {}
	virtual void SetChatViewModel(TSharedRef<class FChatViewModel> InActiveChatViewModel) = 0;
	virtual bool ShouldCaptureFocus() = 0;
	virtual FText GetChatDisconnectText() const = 0;
	virtual EChatMessageType::Type GetMarkupChannel() const = 0;
	virtual FReply HandleChatKeyEntry(const FKeyEvent& KeyEvent) = 0;
	virtual void ValidateChatInput(const FText Message, const FText PlainText) = 0;
	virtual FText GetValidatedInput() = 0;
	virtual bool SendMessage(const FText NewMessage, const FText PlainText) = 0;
	virtual bool IsChatConnected() const = 0;
	virtual TSharedPtr<class FFriendViewModel> GetFriendViewModel(const FString InUserID, const FText Username) = 0;
	virtual void SetFocus() = 0;

	DECLARE_EVENT(FChatEntryViewModel, FChatTextValidatedEvent)
	virtual FChatTextValidatedEvent& OnTextValidated() = 0;

	DECLARE_EVENT(FChatEntryViewModel, FChatEntrySetFocusEvent)
	virtual FChatEntrySetFocusEvent& OnChatListSetFocus() = 0;
};

/**
 * Creates the implementation for an FChatEntryViewModel.
 *
 * @return the newly created FChatEntryViewModel implementation.
 */
FACTORY(TSharedRef< FChatEntryViewModel >, FChatEntryViewModel,
	const TSharedRef<class FMessageService>& MessageService,
	const TSharedRef<class FChatDisplayService>& ChatDisplayService,
	const TSharedPtr<class FFriendsChatMarkupService>& MarkupService,
	const TSharedRef<class FGameAndPartyService>& GamePartyService
	);