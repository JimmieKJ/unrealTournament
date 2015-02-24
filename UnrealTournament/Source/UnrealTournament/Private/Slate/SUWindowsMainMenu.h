// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTMenuBase.h"
#include "Panels/SUWCreateGamePanel.h"
#include "Panels/SUHomePanel.h"
#include "Panels/SUTWebBrowserPanel.h"

const FString CommunityVideoURL = "http://epic.gm/utlaunchertutorial";

#if !UE_SERVER
class SUWindowsMainMenu : public SUTMenuBase
{
protected:
	bool bNeedToShowGamePanel;

	TSharedPtr<SUWCreateGamePanel> GamePanel;
	TSharedPtr<SUTWebBrowserPanel> WebPanel;

	TSharedRef<SWidget> AddPlayNow();

	virtual void CreateDesktop();
	virtual void SetInitialPanel();

	virtual void BuildLeftMenuBar();
	
	virtual TSharedRef<SWidget> BuildTutorialSubMenu();

	virtual FReply OnShowGamePanel(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnTutorialClick();
	virtual FReply OnCloseClicked();

	virtual FReply OnBootCampClick(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnCommunityClick(TSharedPtr<SComboButton> MenuButton);

	virtual FReply OnPlayQuickMatch(TSharedPtr<SComboButton> MenuButton, FName QuickMatchType);

	virtual FReply OnConnectIP(TSharedPtr<SComboButton> MenuButton);
	virtual void ConnectIPDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

	virtual void OpenDelayedMenu();
	virtual bool ShouldShowBrowserIcon();
};
#endif
