// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Base/SUTPanelBase.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

class SUTMatchSummaryPanel;

class UNREALTOURNAMENT_API SUTInGameHomePanel : public SUTPanelBase
{
public:
	virtual void ConstructPanel(FVector2D CurrentViewportSize);

	virtual void OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow);
	virtual void OnHidePanel();

	FReply virtual ChangeChatDestination(TSharedPtr<SComboButton> Button, FName NewDestination);

	void SetChatDestination(FName NewDestination)
	{
		ChatDestination = NewDestination;
	}

	FText GetChatDestinationText() const;
	FText GetChatDestinationTag(FName Destination);

	void FocusChat();

	void ShowMatchSummary(bool bInitial);
	void HideMatchSummary();
	TSharedPtr<SUTMatchSummaryPanel> GetSummaryPanel();

	EVisibility GetSummaryVisibility() const;

	// If true, submitting text chat will close the menu
	bool bCloseOnSubmit;

	bool bShowScoreboard;

protected:

	bool bFocusSummaryInv;


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

	TSharedPtr<SOverlay> SummaryOverlay;
	TSharedPtr<SUTMatchSummaryPanel> SummaryPanel;
	TSharedPtr<STextBlock> TypeMsg;

	virtual void BuildChatDestinationMenu();

	virtual void ChatTextChanged(const FText& NewText);
	virtual void ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType);

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent) override;

	virtual bool GetGameMousePosition(FVector2D& MousePosition);

	virtual void ShowContextMenu(UUTScoreboard* Scoreboard, FVector2D ContextMenuLocation, FVector2D ViewportBounds);
	virtual void HideContextMenu();
	virtual FReply ContextCommand(int32 CommandId, TWeakObjectPtr<AUTPlayerState> TargetPlayerState);

	TSharedPtr<SOverlay> SubMenuOverlay;
	bool bShowingContextMenu;


};

#endif