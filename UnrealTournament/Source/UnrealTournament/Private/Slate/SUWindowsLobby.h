// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Panels/SUMidGameInfoPanel.h"
#include "Panels/SULobbyInfoPanel.h"
#include "Slate/SlateGameResources.h"
#include "SUInGameMenu.h"


#if !UE_SERVER
class SUWindowsLobby : public SUInGameMenu
{

protected:
	
	TSharedPtr<SButton> MatchButton;
	virtual TSharedRef<SWidget> BuildMenuBar();

	virtual void BuildTopBar();
	virtual void BuildDesktop();
	
	FText GetMatchButtonText() const;
	FString GetMatchCount() const;

	FReply MatchButtonClicked();
};

#endif