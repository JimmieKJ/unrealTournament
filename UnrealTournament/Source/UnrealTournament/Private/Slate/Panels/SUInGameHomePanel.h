// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

class SUInGameHomePanel : public SUWPanel
{
public:
	virtual void ConstructPanel(FVector2D CurrentViewportSize);

	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow);
	virtual void OnHidePanel();

	FReply virtual ChangeChatDestination(TSharedPtr<SComboButton> Button, FName NewDestination);

protected:

	FName ChatDestination;

	// This is the portion of the UI that contains the chat area
	TSharedPtr<SVerticalBox> ChatArea;

	// The Vertical Box that makes up the menu
	TSharedPtr<SVerticalBox> ChatMenu;

	// This is the portion of the UI that contains the menu area
	TSharedPtr<SVerticalBox> MenuArea;

	TSharedPtr<SComboButton> ChatDestinationsButton;
	TSharedPtr<SEditableTextBox> ChatText;
	TSharedRef<SWidget> BuildChatDestinationsButton();

	virtual void BuildChatDestinationMenu();

	virtual void ChatTextChanged(const FText& NewText);
	virtual void ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType);
	FText GetChatDestinationText() const;
	FText GetChatDestinationTag(FName Destination);

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent) override;

	virtual bool GetGameMousePosition(FVector2D& MousePosition);


};

#endif