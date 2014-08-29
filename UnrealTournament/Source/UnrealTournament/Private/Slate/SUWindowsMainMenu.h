// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "Slate/SlateGameResources.h"

class SUWindowsMainMenu : public SUWindowsDesktop
{
protected:
	TSharedPtr<class SHorizontalBox> MenuBar;

	virtual void CreateDesktop();

	virtual FReply OnCreateGame(bool bOnline);
	virtual FReply OnConnectIP();
	virtual void ConnectIPDialogResult(const FString& InputText, bool bCancelled);
	virtual FReply OnChangeTeam(int32 NewTeamIndex);
	virtual FReply OpenPlayerSettings();
	virtual FReply OpenSystemSettings();
	virtual FReply OpenTPSReport();
	virtual FReply OpenCredits();
	virtual FReply OnMenuHTTPButton(FString URL);
	virtual FReply OpenControlSettings();

	virtual TSharedRef<SWidget> BuildMenuBar();

	virtual void BuildFileSubMenu();
	virtual void BuildGameSubMenu();
	virtual void BuildOptionsSubMenu();
	virtual void BuildAboutSubMenu();
};

