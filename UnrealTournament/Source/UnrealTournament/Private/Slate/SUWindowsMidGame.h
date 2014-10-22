// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "Panels/SUMidGameInfoPanel.h"
#include "Slate/SlateGameResources.h"

namespace SettingsDialogs
{
	const extern FName SettingPlayer;
	const extern FName SettingsSystem;
	const extern FName SettingsControls;
}


#if !UE_SERVER
class SUWindowsMidGame : public SUWindowsDesktop
{
public:
	virtual void OnMenuOpened();
	virtual void OnMenuClosed();
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

protected:

	TSharedPtr<class SHorizontalBox> MenuBar;
	TSharedPtr<class SUMidGameInfoPanel> InfoPanel;
	TSharedPtr<class STextBlock> LeftStatusText;
	TSharedPtr<class STextBlock> RightStatusText;

	TSharedPtr<class SOverlay> OnlineOverlay;
	virtual void CreateDesktop();

	TSharedPtr<class SImage> PortraitImage;

	virtual TSharedRef<SWidget> BuildMenuBar();

	FText ConvertTime(int Seconds);

	virtual void UpdateOnlineState();

	virtual void BuildInfoSubMenu();
	virtual void BuildPlaySubMenu();
	virtual void BuildTeamSubMenu();
	virtual void BuildServerBrowserSubMenu();
	virtual void BuildOptionsSubMenu();
	virtual void BuildExitMatchSubMenu();

	virtual void BuildInfoPanel();

	FReply OnLogin();

	FReply Disconnect(TSharedPtr<SComboButton> MenuButton);
	FReply Reconnect(TSharedPtr<SComboButton> MenuButton);

	FReply OnInfo();
	FReply OnServerBrowser();
	FReply OpenSettingsDialog(TSharedPtr<SComboButton> MenuButton, FName SettingsToOpen);
	FReply OnChangeTeam(int32 NewTeamIndex,TSharedPtr<SComboButton> MenuButton);
	FReply ExitMatch();
	FReply Play();

	FPlayerOnlineStatusChangedDelegate PlayerOnlineStatusChangedDelegate;
	virtual void OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID);

};

#endif