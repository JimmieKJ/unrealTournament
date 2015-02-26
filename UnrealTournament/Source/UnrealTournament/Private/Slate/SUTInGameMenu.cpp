// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsMainMenu.h"
#include "SUWindowsStyle.h"
#include "SUWDialog.h"
#include "SUWSystemSettingsDialog.h"
#include "SUWPlayerSettingsDialog.h"
#include "SUWWeaponConfigDialog.h"
#include "SUWCreateGameDialog.h"
#include "SUWControlSettingsDialog.h"
#include "SUWInputBox.h"
#include "SUWMessageBox.h"
#include "SUWScaleBox.h"
#include "UTGameEngine.h"
#include "Panels/SUInGameHomePanel.h"

#if !UE_SERVER

/****************************** [ Build Sub Menus ] *****************************************/

void SUTInGameMenu::BuildLeftMenuBar()
{
	if (LeftMenuBar.IsValid())
	{
		AUTGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
		if (GS && GS->bTeamGame)
		{
			LeftMenuBar->AddSlot()
			.Padding(5.0f,0.0f,0.0f,0.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button")
				.OnClicked(this, &SUTInGameMenu::OnTeamChangeClick)
				.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_ChangeTeam","CHANGE TEAM").ToString())
						.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
					]
				]
			];
		}
/*
		LeftMenuBar->AddSlot()
		.Padding(5.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button")
			.OnClicked(this, &SUTInGameMenu::OnSpectateClick)
			.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_Spectate","SPECTATE").ToString())
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
				]
			]
		];
*/
	}
}

void SUTInGameMenu::BuildExitMenu(TSharedPtr<SComboButton> ExitButton, TSharedPtr<SVerticalBox> MenuSpace)
{
	MenuSpace->AddSlot()
	.AutoHeight()
	[
		SNew(SButton)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
		.ContentPadding(FMargin(10.0f, 5.0f))
		.Text(NSLOCTEXT("SUTMenuBase", "MenuBar_Exit_ReturnToGame", "Close Menu").ToString())
		.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
		.OnClicked(this, &SUTInGameMenu::OnCloseMenu, ExitButton)
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
		.Text(NSLOCTEXT("SUTInGameMenu","MenuBar_ReturnToMainMenu","Return to Main Menu"))
		.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
		.OnClicked(this, &SUTInGameMenu::OnReturnToMainMenu, ExitButton)
	];

}

FReply SUTInGameMenu::OnCloseMenu(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	CloseMenus();

	return FReply::Handled();
}

FReply SUTInGameMenu::OnReturnToLobby(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	if (PlayerOwner->LastLobbyServerGUID != TEXT(""))
	{
		CloseMenus();
		// Connect to that GUID
		AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
		if (PC)
		{
			PC->ConnectToServerViaGUID(PlayerOwner->LastLobbyServerGUID, false);
			PlayerOwner->LastLobbyServerGUID = TEXT("");
		}
	}

	return FReply::Handled();
}

FReply SUTInGameMenu::OnReturnToMainMenu(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);	
	CloseMenus();
	PlayerOwner->ReturnToMainMenu();
	return FReply::Handled();
}

FReply SUTInGameMenu::OnTeamChangeClick()
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC)
	{
		PC->SwitchTeam();
	}
	return FReply::Handled();
}

FReply SUTInGameMenu::OnSpectateClick()
{
	ConsoleCommand(TEXT("ChangeTeam 255"));
	return FReply::Handled();
}


void SUTInGameMenu::SetInitialPanel()
{
	SAssignNew(HomePanel, SUInGameHomePanel)
		.PlayerOwner(PlayerOwner);

	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
}



#endif