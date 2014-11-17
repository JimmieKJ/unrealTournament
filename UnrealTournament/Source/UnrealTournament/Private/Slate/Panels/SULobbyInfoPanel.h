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
	FMatchData(AUTLobbyMatchInfo* inInfo, TSharedPtr<SUMatchPanel> inPanel)
	{
		Info = inInfo;
		Panel = inPanel;
	}
private:
	TWeakObjectPtr<AUTLobbyMatchInfo> Info;
	TSharedPtr<SUMatchPanel> Panel;

};

class SULobbyInfoPanel : public SUChatPanel
{
	virtual void ConstructPanel(FVector2D ViewportSize);
	AUTLobbyPlayerState* GetOwnerPlayerState();

protected:

	// Will be true if we are showing the match dock.  It will be set to false when the owner enters a match 
	bool bShowingMatchDock;

	TArray<FMatchData> MatchData;
	TSharedPtr<SHorizontalBox> MatchPanelDock;
	virtual void BuildNonChatPanel();
	virtual void TickNonChatPanel(float DeltaTime);
};

#endif