// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

class SUInGameHomePanel : public SUWPanel
{

	virtual void ConstructPanel(FVector2D CurrentViewportSize);

	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow);
	virtual void OnHidePanel();

protected:

	FName ChatDestination;

	TSharedPtr<SComboButton> ChatDestinationsButton;
	TSharedPtr<SEditableTextBox> ChatText;
	TSharedRef<SWidget> BuildChatDestinationsButton();

	FReply ChangeChatDestination(TSharedPtr<SComboButton> Button, FName NewDestination);
	void ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType);
	FText GetChatDestinationText() const;
};

#endif