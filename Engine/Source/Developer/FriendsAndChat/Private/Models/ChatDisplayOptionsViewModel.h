// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class FChatItemViewModel;

class FChatDisplayOptionsViewModel
	: public TSharedFromThis<FChatDisplayOptionsViewModel>
	, public IChatViewModel
{
public:

	virtual void SetCaptureFocus(bool bCaptureFocus) = 0;
	virtual void SetChannelUserClicked(const TSharedRef<FChatItemViewModel> ChatItemSelected) = 0;
	virtual void SetCurve(FCurveHandle InFadeCurve) = 0;
	virtual const bool ShouldCaptureFocus() const = 0;
	virtual const bool IsChatHidden() = 0;
	virtual TSharedPtr<class FChatViewModel> GetChatViewModel() const = 0;
	virtual const float GetTimeTransparency() const = 0;
	virtual const EVisibility GetTextEntryVisibility() = 0;
	virtual const EVisibility GetConfirmationVisibility() = 0;

	virtual bool SendMessage(const FText NewMessage)= 0;
	virtual void EnumerateChatChannelOptionsList(TArray<EChatMessageType::Type>& OUTChannelType) = 0;

	DECLARE_EVENT(FChatDisplayOptionsViewModel, FChatListSetFocus)
	virtual FChatListSetFocus& OnChatListSetFocus() = 0;
	virtual ~FChatDisplayOptionsViewModel() {}
};

/**
 * Creates the implementation for an ChatDisplayOptionsViewModel.
 *
 * @return the newly created ChatDisplayOptionViewModel implementation.
 */
FACTORY(TSharedRef< FChatDisplayOptionsViewModel >, FChatDisplayOptionsViewModel,
	const TSharedRef<class FChatViewModel>& ChatViewModel);