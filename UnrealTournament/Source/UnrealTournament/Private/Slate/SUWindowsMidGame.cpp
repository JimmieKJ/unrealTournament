// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "Slate.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsMidGame.h"
#include "SUWindowsStyle.h"
#include "SUWSystemSettingsDialog.h"
#include "SUWPlayerSettingsDialog.h"
#include "SUWCreateGameDialog.h"
#include "SUWControlSettingsDialog.h"
#include "SUWInputBox.h"
#include "SUWMessageBox.h"
#include "SUWScaleBox.h"
#include "UTGameEngine.h"

void SUWindowsMidGame::CreateDesktop()
{
	MenuBar = NULL;

	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()		// This is the Buffer at the Top
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SCanvas)
			]

			+ SVerticalBox::Slot()		// Server Info, MOTD
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SCanvas)
			]

			+ SVerticalBox::Slot()		// Chat
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SCanvas)
			]

			+ SVerticalBox::Slot()		// Menu
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SBox)
				.HeightOverride(70)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MidGameMenuBar"))
						]
					]
					+SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()		// This is the Buffer at the Top
						.AutoHeight()
						.HAlign(HAlign_Fill)
						[
							SNew(SBox)
							.HeightOverride(7)
							[
								SNew(SCanvas)
							]
						]
						+ SVerticalBox::Slot()		// This is the Buffer at the Top
						.AutoHeight()
						.HAlign(HAlign_Right)
						[
							SNew(SBox)
							.HeightOverride(63)
							[
								BuildMenuBar()
							]
						]
					]
				]
			]
		];
}

/****************************** [ Build Sub Menus ] *****************************************/

TSharedRef<SWidget> SUWindowsMidGame::BuildMenuBar()
{
	SAssignNew(MenuBar, SHorizontalBox);
	if (MenuBar.IsValid())
	{
		BuildTeamSubMenu();
		BuildServerBrowserSubMenu();
		BuildOptionsSubMenu();
		BuildExitMatchSubMenu();
	}

	return MenuBar.ToSharedRef();
}

void SUWindowsMidGame::BuildTeamSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	AUTGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	bool bTeamGame = (GS != NULL && GS->bTeamGame);

	if (bTeamGame)
	{
		SAssignNew(DropDownButton, SComboButton)
		.Method(SMenuAnchor::UseCurrentWindow)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton")
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Teams", "CHANGE TEAMS").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton.TextStyle")
		];
	}
	else
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		bool bSpectator = (PS != NULL && PS->bIsSpectator);

		if (bSpectator)
		{
			SAssignNew(DropDownButton, SComboButton)
			.Method(SMenuAnchor::UseCurrentWindow)
			.HasDownArrow(false)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton")
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Teams_PLAY", "PLAY").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton.TextStyle")
			];
		}
		else
		{
			SAssignNew(DropDownButton, SComboButton)
			.Method(SMenuAnchor::UseCurrentWindow)
			.HasDownArrow(false)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton")
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Teams_Spectate", "SPECTATE").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton.TextStyle")
			];
		}
	}

	MenuBar->AddSlot()
		.AutoWidth()
		.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		.HAlign(HAlign_Right)
		[
			DropDownButton.ToSharedRef()
		];
}

void SUWindowsMidGame::BuildServerBrowserSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
		.Method(SMenuAnchor::UseCurrentWindow)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton")
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Browser", "SERVER BROWSER").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton.TextStyle")
		];

	MenuBar->AddSlot()
		.AutoWidth()
		.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		.HAlign(HAlign_Right)
		[
			DropDownButton.ToSharedRef()
		];
}

void SUWindowsMidGame::BuildOptionsSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
		.Method(SMenuAnchor::UseCurrentWindow)
		.HasDownArrow(false)
		.MenuPlacement(MenuPlacement_AboveAnchor)
		.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton")
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options", "OPTIONS").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton.TextStyle")
		];

	DropDownButton->SetMenuContent
		(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_PlayerSettings", "Player Settings").ToString())
//			.OnClicked(this, &SUWindowsDesktop::OpenPlayerSettings)
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_SystemSettings", "System Settings").ToString())
//			.OnClicked(this, &SUWindowsDesktop::OpenSystemSettings)
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ControlSettings", "Control Settings").ToString())
//			.OnClicked(this, &SUWindowsDesktop::OpenControlSettings)
		]
	);



	MenuBar->AddSlot()
		.AutoWidth()
		.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		.HAlign(HAlign_Right)
		[
			DropDownButton.ToSharedRef()
		];
}
void SUWindowsMidGame::BuildExitMatchSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	FText MenuText = PlayerOwner->GetWorld()->GetNetMode() == NM_Standalone ?
		NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_ExitMatch", "EXIT MATCH") :
		NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Disconnect", "DISCONNECT");

	SAssignNew(DropDownButton, SComboButton)
		.Method(SMenuAnchor::UseCurrentWindow)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton")
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(MenuText.ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MidGameMenuButton.TextStyle")
		];

	MenuBar->AddSlot()
		.AutoWidth()
		.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		.HAlign(HAlign_Right)
		[
			DropDownButton.ToSharedRef()
		];
}


