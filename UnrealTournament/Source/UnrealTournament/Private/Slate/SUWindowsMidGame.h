// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "Slate/SlateGameResources.h"

class SUWindowsMidGame : public SUWindowsDesktop
{

protected:

	TSharedPtr<class SHorizontalBox> MenuBar;

	virtual void CreateDesktop();

/*
	virtual FReply OnMenuConsoleCommand(FString Command);
	virtual FReply OnChangeTeam(int32 NewTeamIndex);
	virtual FReply OnOptions(int32 OptionPageIndex);
	virtual FReply OnExit();
*/

	virtual TSharedRef<SWidget> BuildMenuBar();

	virtual void BuildTeamSubMenu();
	virtual void BuildServerBrowserSubMenu();
	virtual void BuildOptionsSubMenu();
	virtual void BuildExitMatchSubMenu();
};

