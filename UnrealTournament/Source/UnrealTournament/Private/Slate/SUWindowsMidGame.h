// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "Panels/SUMidGameInfoPanel.h"
#include "Slate/SlateGameResources.h"
#include "SUInGameMenu.h"


#if !UE_SERVER
class SUWindowsMidGame : public SUInGameMenu
{
public:
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	virtual void BuildDesktop();

protected:
	TSharedPtr<class STextBlock> LeftStatusText;
	TSharedPtr<class STextBlock> RightStatusText;

	virtual void BuildTopBar();
};

#endif