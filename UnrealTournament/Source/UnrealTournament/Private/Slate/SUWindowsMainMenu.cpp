// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "Slate.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsMainMenu.h"
#include "SUWindowsStyle.h"
#include "SUWSystemSettingsDialog.h"
#include "SUWPlayerSettingsDialog.h"
#include "SUWCreateGameDialog.h"
#include "SUWControlSettingsDialog.h"
#include "SUWInputBox.h"
#include "SUWMessageBox.h"
#include "SUWScaleBox.h"
#include "UTGameEngine.h"
#include "SUWServerBrowser.h"

void SUWindowsMainMenu::CreateDesktop()
{
	MenuBar = NULL;
	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SBox)
				.HeightOverride(26)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MenuBar.Background"))
						]
					]
					+SOverlay::Slot()
					[
						BuildMenuBar()
					]
				]
			]

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UWindows.Desktop.Background"))
				]
				/* -- Remove until I can get a hi-rez logo
				+ SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						[
							SNew(SUWScaleBox)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UWindows.Desktop.Background.Logo"))
							]
						]
					]
				]
				*/
				+ SOverlay::Slot()
				[
					SNew(SCanvas)
					//SNew(SUWServerBrowser)  -- Not ready.. don't use yet :)
				]
			]
		];
}

/****************************** [ Build Sub Menus ] *****************************************/

TSharedRef<SWidget> SUWindowsMainMenu::BuildMenuBar()
{
	SAssignNew(MenuBar, SHorizontalBox);
	if (MenuBar.IsValid())
	{
		BuildFileSubMenu();
		BuildGameSubMenu();
		BuildOptionsSubMenu();
		BuildAboutSubMenu();
		
		// version number
		MenuBar->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 5.0f, 0.0f)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Black)
			.Text(FText::Format(NSLOCTEXT("SUWindowsDesktop", "MenuBar_NetVersion", "Network Version: {Ver}"), FText::FromString(FString::Printf(TEXT("%i"), GetDefault<UUTGameEngine>()->GameNetworkVersion))).ToString())
		];
	}

	return MenuBar.ToSharedRef();
}

/**
 *	TODO: Look at converting these into some form of a generic menu bar system.  It's pretty ugly
 *  right now and can probably be converted in to something simple. But I need to figure out
 *  how to get underlining working so I can setup hotkeys first.
 **/

void SUWindowsMainMenu::BuildFileSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
	.Method(SMenuAnchor::UseCurrentWindow)
	.HasDownArrow(false)
	.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
	.ButtonContent()
	[
		SNew(STextBlock)
		.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_File","File").ToString())
	];
	
	if ( DropDownButton.IsValid() ) 
	{
		TSharedPtr<SVerticalBox> Menu = NULL;
	
		SAssignNew(Menu, SVerticalBox);
		if (Menu.IsValid())
		{
			(*Menu).AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_CreateLocal", "Create Local Game").ToString())
				.OnClicked(this, &SUWindowsMainMenu::OnCreateGame, false)
			];
			(*Menu).AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_CreateOnline", "Create Multiplayer Server").ToString())
				.OnClicked(this, &SUWindowsMainMenu::OnCreateGame, true)
			];

			if (GWorld->GetWorld()->GetNetMode() == NM_Client)
			{
				(*Menu).AddSlot()
				.AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_Disconnect", "Disconnect").ToString())
					.OnClicked(this, &SUWindowsDesktop::OnMenuConsoleCommand, FString(TEXT("Disconnect")))
				];
			}
			if (PlayerOwner->PlayerController != NULL)
			{
				const FURL& LastNetURL = GEngine->GetWorldContextFromWorldChecked(PlayerOwner->PlayerController->GetWorld()).LastRemoteURL;
				if (LastNetURL.Valid && LastNetURL.Host.Len() > 0)
				{
					(*Menu).AddSlot()
					.AutoHeight()
					[
						SNew(SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
						.ContentPadding(FMargin(10.0f, 5.0f))
						.Text(FText::Format(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_Reconnect", "Reconnect to {Addr}"), FText::FromString(FString::Printf(TEXT("%s:%i"), *LastNetURL.Host, LastNetURL.Port))).ToString())
						.OnClicked(this, &SUWindowsDesktop::OnMenuConsoleCommand, FString(TEXT("Reconnect")))
					];
				}
			}

			(*Menu).AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_ConnectIP", "Connect To IP").ToString())
				.OnClicked(this, &SUWindowsMainMenu::OnConnectIP)
			];

			(*Menu).AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_ExitGame", "Exit Game").ToString())
				.OnClicked(this, &SUWindowsDesktop::OnMenuConsoleCommand, FString(TEXT("quit")))
			];

		}
		DropDownButton->SetMenuContent(Menu.ToSharedRef());
	}

	MenuBar->AddSlot()
	.AutoWidth()
	.Padding(FMargin(10.0f,0.0f,0.0f,0.0f))
	.HAlign(HAlign_Fill)
	[
		DropDownButton.ToSharedRef()
	];
}

void SUWindowsMainMenu::BuildGameSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
	.Method(SMenuAnchor::UseCurrentWindow)
	.HasDownArrow(false)
	.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
	.ButtonContent()
	[
		SNew(STextBlock)
		.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_Game","Game").ToString())
	];
	
	if ( DropDownButton.IsValid() ) 
	{
		TSharedPtr<SVerticalBox> Menu = NULL;
	
		SAssignNew(Menu, SVerticalBox);
		if (Menu.IsValid() && PlayerOwner != NULL && PlayerOwner->PlayerController != NULL)
		{

			AUTGameState* GS = GWorld->GetWorld()->GetGameState<AUTGameState>();
			if (GS != NULL && GS->bTeamGame)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
				if (PS != NULL && PS->Team != NULL)
				{
					if (PS->Team->GetTeamNum() == 0)
					{
						(*Menu).AddSlot()
						.AutoHeight()
						[
							SNew(SButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
							.ContentPadding(FMargin(10.0f, 5.0f))
							.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Game_ChangeTeamBlue", "Switch to Blue").ToString())
							.OnClicked(this, &SUWindowsMainMenu::OnChangeTeam, 1)
						];
					}
					else
					{
						(*Menu).AddSlot()
						.AutoHeight()
						[
							SNew(SButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
							.ContentPadding(FMargin(10.0f, 5.0f))
							.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Game_ChangeTeamRed", "Switch to Red").ToString())
							.OnClicked(this, &SUWindowsMainMenu::OnChangeTeam, 0)
						];
					}
				}
			}

			(*Menu).AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Game_Suicide", "Suicide").ToString())
				.OnClicked(this, &SUWindowsDesktop::OnMenuConsoleCommand, FString(TEXT("suicide")))
			];
		}
		DropDownButton->SetMenuContent(Menu.ToSharedRef());
	}

	MenuBar->AddSlot()
	.AutoWidth()
	.Padding(FMargin(10.0f,0.0f,0.0f,0.0f))
	.HAlign(HAlign_Fill)
	[
		DropDownButton.ToSharedRef()
	];
}

void SUWindowsMainMenu::BuildOptionsSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
		.Method(SMenuAnchor::UseCurrentWindow)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options", "Options").ToString())
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
			.OnClicked(this, &SUWindowsMainMenu::OpenPlayerSettings)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_SystemSettings", "System Settings").ToString())
			.OnClicked(this, &SUWindowsMainMenu::OpenSystemSettings)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ControlSettings", "Control Settings").ToString())
			.OnClicked(this, &SUWindowsMainMenu::OpenControlSettings)
		]
	);

	MenuBar->AddSlot()
		.AutoWidth()
		.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		.HAlign(HAlign_Fill)
		[
			DropDownButton.ToSharedRef()
		];
}

void SUWindowsMainMenu::BuildAboutSubMenu()
{
	MenuBar->AddSlot()
	.AutoWidth()
	.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
	.HAlign(HAlign_Fill)
	[
		SNew(SComboButton)
		.Method(SMenuAnchor::UseCurrentWindow)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About", "About").ToString())
		]
		.MenuContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_TPSReport", "Third Party Software").ToString())
				.OnClicked(this, &SUWindowsMainMenu::OpenTPSReport)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_Credits", "Credits").ToString())
				.OnClicked(this, &SUWindowsMainMenu::OpenCredits)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_UTSite", "UnrealTournament.com").ToString())
				.OnClicked(this, &SUWindowsMainMenu::OnMenuHTTPButton, FString(TEXT("http://www.unrealtournament.com/")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_UTForums", "Forums").ToString())
				.OnClicked(this, &SUWindowsMainMenu::OnMenuHTTPButton, FString(TEXT("http://forums.unrealtournament.com/")))
			]
		]
	];
}

FReply SUWindowsMainMenu::OnChangeTeam(int32 NewTeamIndex)
{
	return OnMenuConsoleCommand(FString::Printf(TEXT("changeteam %i"), NewTeamIndex));
}

FReply SUWindowsMainMenu::OpenPlayerSettings()
{
	PlayerOwner->OpenDialog(SNew(SUWPlayerSettingsDialog).PlayerOwner(PlayerOwner));
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OpenSystemSettings()
{
	PlayerOwner->OpenDialog(SNew(SUWSystemSettingsDialog).PlayerOwner(PlayerOwner));
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OpenControlSettings()
{
	PlayerOwner->OpenDialog(SNew(SUWControlSettingsDialog).PlayerOwner(PlayerOwner));
	return FReply::Handled();
}

/** last input to connect IP dialog */
static FString LastIP;

FReply SUWindowsMainMenu::OnConnectIP()
{
	PlayerOwner->OpenDialog(
							SNew(SUWInputBox)
							.DefaultInput(LastIP)
							.OnDialogResult(this, &SUWindowsMainMenu::ConnectIPDialogResult)
							.PlayerOwner(PlayerOwner)
							.MessageTitle(NSLOCTEXT("SUWindowsDesktop", "ConnectToIP", "Connect to IP"))
							.MessageText(NSLOCTEXT("SUWindowsDesktop", "ConnecToIPDesc", "Enter address to connect to:"))
							);
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnCreateGame(bool bOnline)
{
	PlayerOwner->OpenDialog(
							SNew(SUWCreateGameDialog)
							.PlayerOwner(PlayerOwner)
							.IsOnline(bOnline)
							);
	return FReply::Handled();
}

void SUWindowsMainMenu::ConnectIPDialogResult(const FString& InputText, bool bCancelled)
{
	if (!bCancelled && InputText.Len() > 0 && PlayerOwner.IsValid())
	{
		FString AdjustedText = InputText.Replace(TEXT("://"), TEXT(""));
		LastIP = AdjustedText;
		PlayerOwner->ViewportClient->ConsoleCommand(*FString::Printf(TEXT("open %s"), *AdjustedText));
	}
}

FReply SUWindowsMainMenu::OpenTPSReport()
{
	PlayerOwner->OpenDialog(
							SNew(SUWMessageBox)
							.PlayerOwner(PlayerOwner)
							.MaxWidthPct(0.8f)
							.MessageTitle(NSLOCTEXT("SUWindowsDesktop", "TPSReportTitle", "Third Party Software"))
							.MessageText(NSLOCTEXT("SUWindowsDesktop", "TPSReportText", "TPSText"))
							.ButtonsMask(UTDIALOG_BUTTON_OK)
							);
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OpenCredits()
{
	PlayerOwner->OpenDialog(
							SNew(SUWMessageBox)
							.PlayerOwner(PlayerOwner)
							.MessageTitle(NSLOCTEXT("SUWindowsDesktop", "CreditsTitle", "Credits"))
							.MessageText(NSLOCTEXT("SUWindowsDesktop", "CreditsText", "Coming Soon!"))
							.ButtonsMask(UTDIALOG_BUTTON_OK)
							);
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnMenuHTTPButton(FString URL)
{
	FString Error;
	FPlatformProcess::LaunchURL(*URL, NULL, &Error);
	if (Error.Len() > 0)
	{
		PlayerOwner->OpenDialog(
								SNew(SUWMessageBox)
								.PlayerOwner(PlayerOwner)
								.MessageTitle(NSLOCTEXT("SUWindowsDesktop", "HTTPBrowserError", "Error Launching Browser"))
								.MessageText(FText::FromString(Error))
								.ButtonsMask(UTDIALOG_BUTTON_OK)
								);
	}
	return FReply::Handled();
}
