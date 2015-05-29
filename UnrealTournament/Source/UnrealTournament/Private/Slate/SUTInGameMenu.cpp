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
#include "SUWHUDSettingsDialog.h"
#include "SUWWeaponConfigDialog.h"
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
						.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_ChangeTeam","CHANGE TEAM"))
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
		.Text(NSLOCTEXT("SUTMenuBase", "MenuBar_Exit_ReturnToGame", "Close Menu"))
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

	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if ( GameState && GameState->HubGuid.IsValid() )
	{
		MenuSpace->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUTInGameMenu", "MenuBar_ReturnToLobby", "Return to Hub"))
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUTInGameMenu::OnReturnToLobby, ExitButton)
		];
	}
	
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
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if ( GameState && GameState->HubGuid.IsValid() )
	{
		CloseMenus();
		AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
		if (PC)
		{
			PC->ConnectToServerViaGUID(GameState->HubGuid.ToString(), false, true);
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
	SAssignNew(HomePanel, SUInGameHomePanel, PlayerOwner);

	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
}

FReply SUTInGameMenu::OpenHUDSettings(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	CloseMenus();
	PlayerOwner->ShowHUDSettings();
	return FReply::Handled();
}


TSharedRef<SWidget> SUTInGameMenu::BuildOptionsSubMenu()
{

	TSharedPtr<SComboButton> DropDownButton = NULL;

	SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
	.AutoWidth()
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.WidthOverride(48)
			.HeightOverride(48)
			[
				SAssignNew(DropDownButton, SComboButton)
				.HasDownArrow(false)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button")
				.ContentPadding(FMargin(0.0f, 0.0f))
				.ButtonContent()
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Settings"))
				]
			]
		]
	];

	DropDownButton->SetMenuContent
	(
		SNew(SBorder)
		.BorderImage(SUWindowsStyle::Get().GetBrush("UT.ContextMenu.Background"))
		.Padding(0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_PlayerSettings", "Player Settings").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUTMenuBase::OpenPlayerSettings, DropDownButton)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_SocialSettings", "Social Settings").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUTMenuBase::OpenSocialSettings, DropDownButton)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_WeaponSettings", "Weapon Settings").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUTMenuBase::OpenWeaponSettings, DropDownButton)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_HUDSettings", "HUD Settings").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUTInGameMenu::OpenHUDSettings, DropDownButton)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_SystemSettings", "System Settings").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUTMenuBase::OpenSystemSettings, DropDownButton)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ControlSettings", "Control Settings").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUTMenuBase::OpenControlSettings, DropDownButton)
			]

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.Padding(FMargin(0.0f, 2.0f))
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush("UT.ContextMenu.Spacer"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ClearCloud", "Clear Game Settings"))
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUTMenuBase::ClearCloud, DropDownButton)
			]
		]
	);

	MenuButtons.Add(DropDownButton);
	return DropDownButton.ToSharedRef();

}


#endif