// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "Panels/SUWCreateGamePanel.h"
#include "Panels/SUHomePanel.h"

#if !UE_SERVER
class SUWindowsMainMenu : public SUWindowsDesktop
{
public:
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

protected:
	TSharedPtr<class SHorizontalBox> LeftMenuBar;
	TSharedPtr<class SHorizontalBox> RightMenuBar;
	TArray< TSharedPtr<SComboButton> > MenuButtons;

	TSharedPtr<SUHomePanel> HomePanel;
	TSharedPtr<SUWCreateGamePanel> GamePanel;

	virtual void CreateDesktop();

	virtual FReply OnShowServerBrowser(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnChangeTeam(int32 NewTeamIndex, TSharedPtr<SComboButton> MenuButton);
	virtual FReply OpenPlayerSettings(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OpenSystemSettings(TSharedPtr<SComboButton> MenuButton);
	virtual FReply ClearCloud(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OpenTPSReport(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OpenCredits(TSharedPtr<SComboButton> MenuButton);
	virtual FReply OnMenuHTTPButton(FString URL, TSharedPtr<SComboButton> MenuButton);
	virtual FReply OpenControlSettings(TSharedPtr<SComboButton> MenuButton);

	virtual FReply OnOnlineClick();


	virtual TSharedRef<SWidget> BuildLeftMenuBar();
	virtual TSharedRef<SWidget> BuildRightMenuBar();

	virtual TSharedRef<SWidget> BuildOptionsSubMenu();
	virtual TSharedRef<SWidget> BuildAboutSubMenu();
	virtual TSharedRef<SWidget> BuildOnlinePresence();
	
	virtual void OnOwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID);

	virtual FReply OnLeaveMatch();
	virtual FReply OnShowHomePanel();
	virtual FReply OnShowGamePanel();
	virtual FReply OnShowServerBrowserPanel();
	virtual FReply ToggleFriendsAndFamily();
	virtual FReply OnShowStatsViewer();
	
protected:
	bool bNeedToShowGamePanel;
	bool bShowingFriends;
	int TickCountDown;
	virtual void ShowGamePanel();

};
#endif
