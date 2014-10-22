// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "Panels/SUMidGameInfoPanel.h"
#include "Panels/SULobbyInfoPanel.h"
#include "Slate/SlateGameResources.h"



#if !UE_SERVER
class SUWindowsLobby : public SUWindowsMidGame
{

protected:
	
	TSharedPtr<SButton> MatchButton;

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	virtual TSharedRef<SWidget> BuildMenuBar();
	virtual void BuildInfoPanel();
	
	FReply MatchButtonClicked();
};

#endif