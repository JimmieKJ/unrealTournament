// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUInGameHomePanel.h"
#include "SULobbyMatchSetupPanel.h"
#include "SUPlayerListPanel.h"
#include "SUTextChatPanel.h"
#include "SUMatchPanel.h"
#include "UTLobbyMatchInfo.h"

#if !UE_SERVER


class UNREALTOURNAMENT_API SULobbyInfoPanel : public SUWPanel
{
public:
	virtual void ConstructPanel(FVector2D ViewportSize);

protected:
	TSharedPtr<SOverlay> MainOverlay;
	TSharedPtr<SVerticalBox> LeftPanel;
	TSharedPtr<SVerticalBox> RightPanel;

	// TODO: Add a SP for the Chat panel when it's written.
	TSharedPtr<SUMatchPanel> MatchBrowser;
	TSharedPtr<SULobbyMatchSetupPanel> MatchSetup;
	TSharedPtr<SUPlayerListPanel> PlayerListPanel;
	TSharedPtr<SUTextChatPanel> TextChatPanel;


protected:

	// Will be true if we are showing the match dock.  It will be set to false when the owner enters a match 
	bool bShowingMatchBrowser;

	// Holds the last match state if the player owner has a current match.  It's used to detect state changes and rebuild the menu as needed.
	FName LastMatchState;
	
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	/**
	 *	Builds the chat window and player list
	 **/
	virtual void BuildChatAndPlayerList();

	void ShowMatchSetupPanel();

	/**
	 *	Builds the Match Browser
	 **/
	void ShowMatchBrowser();

	virtual void ChatDestionationChanged(FName NewDestination);
	virtual void RulesetChanged();
	virtual void PlayerClicked(FUniqueNetIdRepl PlayerId);
};

#endif