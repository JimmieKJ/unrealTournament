// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMatchPanel.h"
#include "UTLobbyMatchInfo.h"

#if !UE_SERVER

class FMatchData
{
public:
	TWeakObjectPtr<AUTLobbyMatchInfo> Info;
	TSharedPtr<SUMatchPanel> Panel;

	FMatchData(AUTLobbyMatchInfo* inInfo, TSharedPtr<SUMatchPanel> inPanel)
	{
		Info = inInfo;
		Panel = inPanel;
	}
	
	static TSharedRef<FMatchData> Make(AUTLobbyMatchInfo* inInfo, TSharedPtr<SUMatchPanel> inPanel)
	{
		return MakeShareable( new FMatchData( inInfo, inPanel ) );
	}


};

class SULobbyInfoPanel : public SUChatPanel
{
	virtual void ConstructPanel(FVector2D ViewportSize);
	AUTLobbyPlayerState* GetOwnerPlayerState();

protected:

	// Will be true if we are showing the match dock.  It will be set to false when the owner enters a match 
	bool bShowingMatchDock;

	// Holds the last match state if the player owner has a current match.  It's used to detect state changes and rebuild the menu as needed.
	FName LastMatchState;

	TArray<TSharedPtr<FMatchData>> MatchData;
	TSharedPtr<SHorizontalBox> MatchPanelDock;
	virtual void BuildNonChatPanel();
	virtual void BuildEmptyMatchMessage();
	virtual void TickNonChatPanel(float DeltaTime);

	void OwnerCurrentMatchChanged(AUTLobbyPlayerState* LobbyPlayerState);
	bool AlreadyTrackingMatch(AUTLobbyMatchInfo* TestMatch);
	bool MatchStillValid(FMatchData* Match);
};

#endif