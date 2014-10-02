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

	virtual void OnMenuOpened();
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

protected:

	TSharedPtr<class SHorizontalBox> MenuBar;
	TSharedPtr<class SUMidGameInfoPanel> InfoPanel;
	TSharedPtr<class STextBlock> StatusText;
	TSharedPtr<class STextBlock> ClockText;

	virtual void CreateDesktop();

	virtual TSharedRef<SWidget> BuildMenuBar();

	FText ConvertTime(int Seconds);

	virtual void BuildTeamSubMenu();
	virtual void BuildServerBrowserSubMenu();
	virtual void BuildOptionsSubMenu();
	virtual void BuildExitMatchSubMenu();

	FReply OnInfo();
	FReply OnServerBrowser();
	FReply OpenSettingsDialog(TSharedPtr<SComboButton> MenuButton, FName SettingsToOpen);
	FReply OnChangeTeam(int32 NewTeamIndex,TSharedPtr<SComboButton> MenuButton);
	FReply ExitMatch();
};

#endif