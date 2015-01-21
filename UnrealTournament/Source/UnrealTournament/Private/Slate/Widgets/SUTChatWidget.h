// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#include "FortniteGame/Public/UI/FortHUDPCTrackerBase.h"

//class declare
class SFortChatWidget : public SCompoundWidget, public FortHUDPCTrackerBase
{
public:
	SLATE_BEGIN_ARGS(SFortChatWidget)
		: _bKeepVisible(false)
	{}

	/** Should the chat widget be kept visible at all times */
	SLATE_ARGUMENT(bool, bKeepVisible)

	SLATE_END_ARGS()

	/** Needed for every widget */
	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx);

	/** Gets the visibility of the entry widget */
	virtual EVisibility GetEntryVisibility() const;

	/** Sets the visibility of the entry widget and associated logic */
	virtual void SetEntryVisibility( TAttribute<EVisibility> InVisibility );

	/** Handle Friends network message sent */
	void HandleFriendsNetworkChatMessage(const FString& NetworkMessage);

	/** Handle Friends chat list updated */
	void HandleChatListUpdated();

	/** Focus edit box so user can immediately start typing */
	void SetFocus();

protected:

	/** Update function. Kind of a hack. Allows us to focus keyboard. */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** The UI sets up the appropriate mouse settings upon focus */
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;

	/** Callback for when we commit some text */
	void OnChatTextCommitted();

	/** Visibility of the entry widget previous frame */
	EVisibility LastVisibility;

	/** Time to show the chat lines as visible after receiving a chat message */
	double ChatFadeTime;

	/** When we received our last chat message */
	double LastChatLineTime;

	/** The edit text widget */
	TSharedPtr< SBorder > ChatEditBox;

	/** Should this chatbox be kept visible */
	bool bKeepVisible;

	// Holds the chat widget view model
	TSharedPtr<IChatViewModel> ViewModel;
};




