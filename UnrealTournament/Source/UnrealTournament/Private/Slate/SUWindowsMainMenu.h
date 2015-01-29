// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"

#if !UE_SERVER
class SUWindowsMainMenu : public SUWindowsDesktop
{
protected:
	TSharedPtr<class SHorizontalBox> MenuBar;
	TArray< TSharedPtr<SComboButton> > MenuButtons;

	virtual void CreateDesktop();

	virtual FReply OnShowServerBrowser(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnShowStatsViewer(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnCreateGame(bool bOnline,TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnConnectIP(TSharedPtr<SComboButton> MenuButton);
	virtual void ConnectIPDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	virtual FReply OnChangeTeam(int32 NewTeamIndex, TSharedPtr<SComboButton> MenuButton);
	virtual FReply OpenPlayerSettings(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OpenSystemSettings(TSharedPtr<SComboButton> MenuButton);
	virtual FReply ClearCloud(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OpenTPSReport(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OpenCredits(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnMenuHTTPButton(FString URL, TSharedPtr<SComboButton> MenuButton);
	virtual FReply OpenControlSettings(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnLeaveMatch(TSharedPtr<SComboButton> MenuButton);

	virtual TSharedRef<SWidget> BuildMenuBar();

	virtual void BuildFileSubMenu();
	virtual void BuildGameSubMenu();
	virtual void BuildOptionsSubMenu();
	virtual void BuildAboutSubMenu();
};
#endif
