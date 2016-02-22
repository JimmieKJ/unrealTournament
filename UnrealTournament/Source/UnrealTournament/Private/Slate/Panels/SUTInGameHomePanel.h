// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Base/SUTPanelBase.h"
#include "../Widgets/SUTChatBar.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

class SUTMatchSummaryPanel;

class UNREALTOURNAMENT_API SUTInGameHomePanel : public SUTPanelBase
{
public:
	virtual void ConstructPanel(FVector2D CurrentViewportSize);

	virtual void OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow);
	virtual void OnHidePanel();

	void ShowMatchSummary(bool bInitial);
	void HideMatchSummary();
	TSharedPtr<SUTMatchSummaryPanel> GetSummaryPanel();

	EVisibility GetSummaryVisibility() const;

	// If true, submitting text chat will close the menu
	bool bCloseOnSubmit;

	virtual TSharedPtr<SWidget> GetInitialFocus();

protected:

	bool bFocusSummaryInv;


	// This is the portion of the UI that contains the chat area
	TSharedPtr<SUTChatBar> ChatBar;
	
	TSharedPtr<SVerticalBox> ChatArea;

	// This is the portion of the UI that contains the menu area
	TSharedPtr<SVerticalBox> MenuArea;

	TSharedPtr<SOverlay> SummaryOverlay;
	TSharedPtr<SUTMatchSummaryPanel> SummaryPanel;

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