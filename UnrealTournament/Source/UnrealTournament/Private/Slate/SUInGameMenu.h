// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWPanel.h"

namespace SettingsDialogs
{
	const extern FName SettingPlayer;
	const extern FName SettingWeapon;
	const extern FName SettingsSystem;
	const extern FName SettingsControls;
	const extern FName SettingsHUD;
}


#if !UE_SERVER

class SUInGameMenu : public SUWindowsDesktop
{
public:
	virtual void OnMenuOpened();
	virtual void OnMenuClosed();

protected:

	// The overlay for the upper status bar.
	TSharedPtr<class SOverlay> TopOverlay;

	// The overlay that contains the bottom menu.  It contains the MenuBar and the Online overlay	
	TSharedPtr<class SOverlay> BottomOverlay;

	// The Menu bar.  This Horz. Box contains all of the menu bar's buttons
	TSharedPtr<class SHorizontalBox> MenuBar;

	// The OnlineOverlay contains all of the needed widgets for managing the players online presence.  This has to be an overlay
	TSharedPtr<class SOverlay> OnlineOverlay;

	// Holds the info panel created by this menu
	TSharedPtr<SUWPanel> DesktopPanel;

	
	// Build the actual menu
	virtual void CreateDesktop();

	TSharedPtr<class SImage> PortraitImage;

	FText ConvertTime(int Seconds);

	virtual void BuildTopBar();
	virtual void BuildDesktop();
	virtual void BuildBottomBar();

	// Build the menu bar
	virtual TSharedRef<SWidget> BuildMenuBar();

	virtual void BuildInfoSubMenu();
	virtual void BuildPlaySubMenu();
	virtual void BuildTeamSubMenu();
	virtual void BuildServerBrowserSubMenu();
	virtual void BuildOptionsSubMenu();
	virtual void BuildExitMatchSubMenu();

	virtual void BuildOnlineBar();

	FReply OnLogin();
	FReply ReturnToLobby(TSharedPtr<SComboButton> MenuButton);
	FReply Disconnect(TSharedPtr<SComboButton> MenuButton);
	FReply Reconnect(TSharedPtr<SComboButton> MenuButton);

	virtual FReply OnInfo();
	virtual FReply OnServerBrowser();
	virtual FReply OpenSettingsDialog(TSharedPtr<SComboButton> MenuButton, FName SettingsToOpen);
	virtual FReply OnChangeTeam(int32 NewTeamIndex,TSharedPtr<SComboButton> MenuButton);
	virtual FReply ExitMatch();
	virtual FReply Play();

	FPlayerOnlineStatusChangedDelegate PlayerOnlineStatusChangedDelegate;
	virtual void OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID);

};

#endif