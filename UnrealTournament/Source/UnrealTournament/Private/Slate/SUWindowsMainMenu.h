// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTMenuBase.h"
#include "Panels/SUWCreateGamePanel.h"
#include "Panels/SUHomePanel.h"

#if !UE_SERVER
class SUWindowsMainMenu : public SUTMenuBase
{
protected:
	bool bNeedToShowGamePanel;

	TSharedPtr<SUWCreateGamePanel> GamePanel;

	TSharedRef<SWidget> AddPlayNow();

	virtual void CreateDesktop();
	virtual void SetInitialPanel();

	virtual void BuildLeftMenuBar();

	virtual FReply OnShowGamePanel();
	virtual FReply OnTutorialClick();
	virtual FReply OnCloseClicked();

	virtual FReply OnPlayQuickMatch(TSharedPtr<SComboButton> MenuButton, FName QuickMatchType);


	virtual void OpenDelayedMenu();

};
#endif
