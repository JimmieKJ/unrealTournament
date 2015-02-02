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
#include "SUWCreateGameDialog.h"
#include "SUWControlSettingsDialog.h"
#include "SUWInputBox.h"
#include "SUWMessageBox.h"
#include "SUWScaleBox.h"
#include "UTGameEngine.h"
#include "Widgets/SUTChatWidget.h"
#include "Widgets/SUTFriendsWidget.h"
#include "Panels/SUWServerBrowser.h"
#include "Panels/SUWStatsViewer.h"
#include "Panels/SUWCreditsPanel.h"

#if !UE_SERVER

void SUWindowsMainMenu::CreateDesktop()
{
	bShowFriendsList = false;

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
				.HeightOverride(49)
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
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()		// This is the Buffer at the Top
						.AutoHeight()
						.HAlign(HAlign_Fill)
						[
							SNew(SBox)
							.HeightOverride(42)
							[
								BuildMenuBar()
							]
						]

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
					]
				]
			]

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(Desktop, SOverlay)
			]
		];

		if (PlayerOwner->IsMenuGame())
		{

			Desktop->AddSlot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						[
							SNew(SUWScaleBox)
							.bMaintainAspectRatio(false)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("NewSchool.Background"))
							]
						]
					]
				];

			Desktop->AddSlot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						[
							SNew(SUWScaleBox)
							.bMaintainAspectRatio(true)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT15.Logo.Overlay"))
							]
						]
					]
				];

			if (FParse::Param(FCommandLine::Get(), TEXT("EnableFriendsAndChat")))
			{
				Desktop->AddSlot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Bottom)
					[
						SNew(SBorder)
						.Padding(0)
						.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
						.Visibility(EVisibility::Visible) //this, &SUWindowsMainMenu::GetChatWidgetVisibility)
						[
							// SAssignNew(ChatWidget, SUTChatWidget, PlayerOwner->PlayerController)
							SNew(SUTChatWidget, PlayerOwner->PlayerController)
						]
					];

				Desktop->AddSlot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Top)
					.Padding(FMargin(15.0f, 30.0f))
					[
						SNew(SBorder)
						.Padding(0)
						.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
						.Visibility_Lambda([this]()->EVisibility{return this->bShowFriendsList ? EVisibility::Visible : EVisibility::Collapsed;})
						[
							SNew(SUTFriendsWidget, PlayerOwner->PlayerController)
						]
					];
			}
		}
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
		.VAlign(VAlign_Fill)
		.Padding(0.0f, 0.0f, 5.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.Padding(FMargin(10.0f,0.0f))
			.HAlign(HAlign_Fill)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
				.Visibility(FParse::Param(FCommandLine::Get(), TEXT("EnableFriendsAndChat")) ? EVisibility::Visible : EVisibility::Collapsed)
				.OnClicked(this, &SUWindowsMainMenu::HandleFriendsButtonClicked)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Friends", "Friends"))
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.TextStyle")	
					]
				]
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(FText::Format(NSLOCTEXT("SUWindowsDesktop", "MenuBar_NetVersion", "Network Version: {Ver}"), FText::FromString(FString::Printf(TEXT("%i"), GetDefault<UUTGameEngine>()->GameNetworkVersion))).ToString())
			]
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
	.HasDownArrow(false)
	.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
	.ButtonContent()
	[
		SNew(STextBlock)
		.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_File","Game").ToString())
		.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.TextStyle")
	];
	
	if ( DropDownButton.IsValid() ) 
	{
		TSharedPtr<SVerticalBox> Menu = NULL;
	
		SAssignNew(Menu, SVerticalBox);
		if (Menu.IsValid())
		{
			if (PlayerOwner->IsMenuGame())
			{
				(*Menu).AddSlot()
				.AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_CreateLocal", "Create Local Game").ToString())
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
					.OnClicked(this, &SUWindowsMainMenu::OnCreateGame, false, DropDownButton)
				];
				(*Menu).AddSlot()
				.AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_CreateOnline", "Create Multiplayer Server").ToString())
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
					.OnClicked(this, &SUWindowsMainMenu::OnCreateGame, true, DropDownButton)
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
					.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_Disconnect", "Leave Match").ToString())
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
					.OnClicked(this, &SUWindowsMainMenu::OnLeaveMatch, DropDownButton)
				];
			}
			

			(*Menu).AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_ServerBrowser", "Server Browser").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")			
				.OnClicked(this, &SUWindowsMainMenu::OnShowServerBrowser, DropDownButton)
			];
			
			(*Menu).AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_StatsViewer", "Stats Viewer").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")			
				.OnClicked(this, &SUWindowsMainMenu::OnShowStatsViewer, DropDownButton)
			];

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
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
						.OnClicked(this, &SUWindowsMainMenu::OnMenuConsoleCommand, FString(TEXT("Reconnect")), DropDownButton)
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
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
				.OnClicked(this, &SUWindowsMainMenu::OnConnectIP, DropDownButton)
			];

			(*Menu).AddSlot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_File_ExitGame", "Exit Game").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
				.OnClicked(this, &SUWindowsMainMenu::OnMenuConsoleCommand, FString(TEXT("quit")), DropDownButton)
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

	MenuButtons.Add(DropDownButton);
}

void SUWindowsMainMenu::BuildGameSubMenu()
{

	if (PlayerOwner->IsMenuGame()) return; // No Game Menu if in the menus

	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
	.HasDownArrow(false)
	.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
	.ButtonContent()
	[
		SNew(STextBlock)
		.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_Game","Game").ToString())
		.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.TextStyle")
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
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
							.OnClicked(this, &SUWindowsMainMenu::OnChangeTeam, 1, DropDownButton)
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
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
							.OnClicked(this, &SUWindowsMainMenu::OnChangeTeam, 0, DropDownButton)
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
				.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
				.OnClicked(this, &SUWindowsMainMenu::OnMenuConsoleCommand, FString(TEXT("suicide")), DropDownButton)
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

	MenuButtons.Add(DropDownButton);
}

void SUWindowsMainMenu::BuildOptionsSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options", "Options").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.TextStyle")
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
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OpenPlayerSettings, DropDownButton)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_SystemSettings", "System Settings").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OpenSystemSettings, DropDownButton)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ControlSettings", "Control Settings").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OpenControlSettings, DropDownButton)
		]
	
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(2)
			[
				SNew(SCanvas)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ClearCloud", "Clear Game Settings").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::ClearCloud, DropDownButton)
		]
	);

	MenuBar->AddSlot()
		.AutoWidth()
		.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		.HAlign(HAlign_Fill)
		[
			DropDownButton.ToSharedRef()
		];

	MenuButtons.Add(DropDownButton);

}

void SUWindowsMainMenu::BuildAboutSubMenu()
{

	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
	.HasDownArrow(false)
	.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
	.ButtonContent()
	[
		SNew(STextBlock)
		.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About", "About").ToString())
		.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.TextStyle")
	];

	// NOTE: If you inline the setting of the menu content during the construction of the ComboButton
	// it doesn't assign the sharedptr until after the whole menu is constructed.  So if for example,
	// like these buttons here you need the value of the sharedptr it won't be available :(

	DropDownButton->SetMenuContent
	(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_TPSReport", "Third Party Software").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OpenTPSReport, DropDownButton)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_Credits", "Credits").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OpenCredits, DropDownButton)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_UTSite", "UnrealTournament.com").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OnMenuHTTPButton, FString(TEXT("http://www.unrealtournament.com/")),DropDownButton)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_UTForums", "Forums").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OnMenuHTTPButton, FString(TEXT("http://forums.unrealtournament.com/")), DropDownButton)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_WR", "Widget Reflector").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OnMenuConsoleCommand, FString(TEXT("widgetreflector")), DropDownButton)
		]
	);


	MenuBar->AddSlot()
	.AutoWidth()
	.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
	.HAlign(HAlign_Fill)
	[
		DropDownButton.ToSharedRef()
	];

	MenuButtons.Add(DropDownButton);
}

FReply SUWindowsMainMenu::OnChangeTeam(int32 NewTeamIndex,TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	
	ConsoleCommand(FString::Printf(TEXT("changeteam %i"), NewTeamIndex));

	return FReply::Handled();

}

FReply SUWindowsMainMenu::OpenPlayerSettings(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	PlayerOwner->OpenDialog(SNew(SUWPlayerSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop","PlayerSettings","Player Settings")));
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OpenSystemSettings(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	PlayerOwner->OpenDialog(SNew(SUWSystemSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop","System","System Settings")));
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OpenControlSettings(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	PlayerOwner->OpenDialog(SNew(SUWControlSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop","Controls","Control Settings")));
	return FReply::Handled();
}

FReply SUWindowsMainMenu::ClearCloud(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	if (PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->ClearProfileSettings();				
	}
	else
	{
		PlayerOwner->ShowMessage(NSLOCTEXT("SUWindowsMainMenu","NotLoggedInTitle","Not Logged In"), NSLOCTEXT("SuWindowsMainMenu","NoLoggedInMsg","You need to be logged in to clear your cloud settings."), UTDIALOG_BUTTON_OK);
	}
	return FReply::Handled();
}


FReply SUWindowsMainMenu::OnConnectIP(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	PlayerOwner->OpenDialog(
							SNew(SUWInputBox)
							.DefaultInput(PlayerOwner->LastConnectToIP)
							.DialogSize(FVector2D(600,200))
							.OnDialogResult(this, &SUWindowsMainMenu::ConnectIPDialogResult)
							.PlayerOwner(PlayerOwner)
							.DialogTitle(NSLOCTEXT("SUWindowsDesktop", "ConnectToIP", "Connect to IP"))
							.MessageText(NSLOCTEXT("SUWindowsDesktop", "ConnecToIPDesc", "Enter address to connect to:"))
							.ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
							);
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnCreateGame(bool bOnline,TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	PlayerOwner->OpenDialog(
							SNew(SUWCreateGameDialog)
							.PlayerOwner(PlayerOwner)
							.IsOnline(bOnline)
							, 0);
	return FReply::Handled();
}

void SUWindowsMainMenu::ConnectIPDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		TSharedPtr<SUWInputBox> Box = StaticCastSharedPtr<SUWInputBox>(Widget);
		if (Box.IsValid())
		{
			FString InputText = Box->GetInputText();
			if (InputText.Len() > 0 && PlayerOwner.IsValid())
			{
				FString AdjustedText = InputText.Replace(TEXT("://"), TEXT(""));
				PlayerOwner->LastConnectToIP = AdjustedText;
				PlayerOwner->SaveConfig();
				PlayerOwner->ViewportClient->ConsoleCommand(*FString::Printf(TEXT("open %s"), *AdjustedText));
				CloseMenus();
			}
		}
	}
}

FReply SUWindowsMainMenu::OpenTPSReport(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	PlayerOwner->OpenDialog(
							SNew(SUWMessageBox)
							.PlayerOwner(PlayerOwner)
							.DialogSize(FVector2D(0.7, 0.8))
							.bDialogSizeIsRelative(true)
							.DialogTitle(NSLOCTEXT("SUWindowsDesktop", "TPSReportTitle", "Third Party Software"))
							.MessageText(NSLOCTEXT("SUWindowsDesktop", "TPSReportText", "TPSText"))
							.MessageTextStyleName("UWindows.Standard.Dialog.TextStyle.Legal")
							.ButtonMask(UTDIALOG_BUTTON_OK)
							);
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OpenCredits(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	TSharedPtr<class SUWCreditsPanel> CreditsPanel = PlayerOwner->GetCreditsPanel();
	if (CreditsPanel.IsValid())
	{
		ActivatePanel(CreditsPanel);
	}
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnMenuHTTPButton(FString URL,TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	FString Error;
	FPlatformProcess::LaunchURL(*URL, NULL, &Error);
	if (Error.Len() > 0)
	{
		PlayerOwner->OpenDialog(
								SNew(SUWMessageBox)
								.PlayerOwner(PlayerOwner)
								.DialogTitle(NSLOCTEXT("SUWindowsDesktop", "HTTPBrowserError", "Error Launching Browser"))
								.MessageText(FText::FromString(Error))
								.ButtonMask(UTDIALOG_BUTTON_OK)
								);
	}
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnShowServerBrowser(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	TSharedPtr<class SUWServerBrowser> Browser = PlayerOwner->GetServerBrowser();
	if (Browser.IsValid())
	{
		ActivatePanel(Browser);
	}
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnShowStatsViewer(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	TSharedPtr<class SUWStatsViewer> StatsViewer = PlayerOwner->GetStatsViewer();
	if (StatsViewer.IsValid())
	{
		ActivatePanel(StatsViewer);
	}
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnLeaveMatch(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	PlayerOwner->HideMenu();
	ConsoleCommand(TEXT("open UT-Entry?Game=/Script/UnrealTournament.UTMenuGameMode"));
	return FReply::Handled();
}

FReply SUWindowsMainMenu::HandleFriendsButtonClicked()
{
	bShowFriendsList = !bShowFriendsList;
	return FReply::Handled();
}

#endif