// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"


#if !UE_SERVER

class SULobbyInfoPanel : public SUMidGameInfoPanel
{
	virtual void BuildPage(FVector2D ViewportSize);
	virtual void OnShowPanel();
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

protected:

	TSharedPtr<SHorizontalBox> MatchPanels;
	virtual void AddInfoPanel();
};

#endif