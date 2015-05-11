// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#if !UE_SERVER
class IChatViewModel;

class UNREALTOURNAMENT_API SUTChatWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUTChatWidget)
	{}

	SLATE_END_ARGS()

	/** Needed for every widget */
	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx);

	/** Focus edit box so user can immediately start typing */
	void SetFocus();

private:
	/** Animate the chat widget size as it fades */
	FOptionalSize GetChatWidgetHeight() const;

	/** Handle Friends network message sent */
	void HandleFriendsNetworkChatMessage(const FString& NetworkMessage);

	/** Handle Friends chat list updated */
	void HandleChatListUpdated();

	/** Update function. Kind of a hack. Allows us to focus keyboard. */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** The UI sets up the appropriate mouse settings upon focus */
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;

	/** Callback for when we commit some text */
	void OnChatTextCommitted();

	/** Visibility of the entry widget previous frame */
	EVisibility LastVisibility;

	/** The edit text widget */
	TSharedPtr<SBorder> ChatEditBox;

	/** Holds the chat widget view model */
	TSharedPtr<IChatViewModel> ViewModel;

	/** Player context */
	FLocalPlayerContext Ctx;
};
#endif