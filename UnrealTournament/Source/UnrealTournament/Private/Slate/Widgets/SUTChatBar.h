// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "../SUTStyle.h"

#if !UE_SERVER

class UUTLocalPlayer;
class SUTChatEditBox;

class UNREALTOURNAMENT_API SUTChatBar: public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUTChatBar)
		{}

		SLATE_ARGUMENT(FName, InitialChatDestination)
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> inPlayerOwner);
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

protected:
	TWeakObjectPtr<UUTLocalPlayer> PlayerOwner;

	FOnTextCommitted ExternalOnTextCommitted;

	FText GetChatDestinationText() const;

	// Where is this chat text going
	FName ChatDestination;

	// This is the portion of the UI that contains the chat area
	TSharedPtr<SHorizontalBox> ChatArea;

	// The Horizontal Box that will contain the slot with the Chat Widget. The Chat widget comes from
	// the Local player.
	TSharedPtr<SHorizontalBox> ChatSlot;

	// The Vertical Box that makes up the menu
	TSharedPtr<SVerticalBox> ChatMenu;

	// Holds the combo button that allows you to pick the chat destination
	TSharedPtr<SComboButton> ChatDestinationsButton;

	TSharedRef<SWidget> BuildChatDestinationsButton();
	void BuildChatDestinationMenu();
	FReply ChangeChatDestination(TSharedPtr<SComboButton> Button, FName NewDestination);
	void ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType);
};

#endif