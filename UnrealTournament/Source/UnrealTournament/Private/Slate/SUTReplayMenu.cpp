// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTReplayMenu.h"
#include "Panels/SUWReplayBrowser.h"

#if !UE_SERVER

/****************************** [ Build Sub Menus ] *****************************************/

void SUTReplayMenu::BuildLeftMenuBar()
{
	if (LeftMenuBar.IsValid())
	{
		LeftMenuBar->AddSlot()
		.Padding(40.0f, 0.0f, 0.0f, 0.0f)
		.AutoWidth()
		[
			BuildWatchSubMenu()
		];
	}
}


void SUTReplayMenu::BuildExitMenu(TSharedPtr<SComboButton> ExitButton, TSharedPtr<SVerticalBox> MenuSpace)
{
	MenuSpace->AddSlot()
	.AutoHeight()
	[
		SNew(SButton)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
		.ContentPadding(FMargin(10.0f, 5.0f))
		.Text(NSLOCTEXT("SUTMenuBase", "MenuBar_Exit_ReturnToGame", "Close Menu"))
		.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
		.OnClicked(this, &SUTReplayMenu::OnCloseMenu, ExitButton)
	];

	MenuSpace->AddSlot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Fill)
	.Padding(FMargin(0.0f, 2.0f))
	[
		SNew(SImage)
		.Image(SUWindowsStyle::Get().GetBrush("UT.ContextMenu.Spacer"))
	];

	MenuSpace->AddSlot()
	.AutoHeight()
	[
		SNew(SButton)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
		.ContentPadding(FMargin(10.0f, 5.0f))
		.Text(NSLOCTEXT("SUTInGameMenu", "MenuBar_ReturnToMainMenu", "Return to Main Menu"))
		.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
		.OnClicked(this, &SUTReplayMenu::OnReturnToMainMenu, ExitButton)
	];
}

FReply SUTReplayMenu::OnCloseMenu(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	CloseMenus();

	return FReply::Handled();
}

FReply SUTReplayMenu::OnReturnToMainMenu(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	CloseMenus();
	PlayerOwner->ReturnToMainMenu();
	return FReply::Handled();
}

#endif