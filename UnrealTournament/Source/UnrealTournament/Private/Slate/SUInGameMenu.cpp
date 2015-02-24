// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUInGameMenu.h"
#include "SUWindowsStyle.h"
#include "SUWSystemSettingsDialog.h"
#include "SUWPlayerSettingsDialog.h"
#include "SUWWeaponConfigDialog.h"
#include "SUWCreateGameDialog.h"
#include "SUWControlSettingsDialog.h"
#include "SUWInputBox.h"
#include "SUWMessageBox.h"
#include "SUWScaleBox.h"
#include "UTGameEngine.h"
#include "Panels/SUWServerBrowser.h"
#include "UTLobbyGameState.h"

namespace SettingsDialogs
{
	const FName SettingPlayer = FName(TEXT("SettingsPlayern"));
	const FName SettingWeapon = FName(TEXT("SettingsWeapon"));
	const FName SettingsSystem = FName(TEXT("SettingsSystem"));
	const FName SettingsControls = FName(TEXT("SettingsControls"));
	const FName SettingsHUD = FName(TEXT("SettingsHUD"));
}

#if !UE_SERVER
void SUInGameMenu::CreateDesktop()
{
	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SOverlay)

			// The Background
			+SOverlay::Slot()
			[
				// Shade the game a bit
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.DarkBackground"))
			]

			// The Guts
			+SOverlay::Slot()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()		// This is the Buffer at the Top
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				[
					SAssignNew(TopOverlay, SOverlay)
				]

				+ SVerticalBox::Slot()		// This is the Buffer at the Top
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(Desktop, SOverlay)
				]

				+ SVerticalBox::Slot()		// Menu
				.AutoHeight()
				.HAlign(HAlign_Fill)
				[
					SAssignNew(BottomOverlay, SOverlay)
				]
			]
		];

	BuildTopBar();
	BuildBottomBar();
}

FText SUInGameMenu::ConvertTime(int Seconds)
{
	int Hours = Seconds / 3600;
	Seconds -= Hours * 3600;
	int Mins = Seconds / 60;
	Seconds -= Mins * 60;

	FFormatNamedArguments Args;
	FNumberFormattingOptions Options;

	Options.MinimumIntegralDigits = 2;
	Options.MaximumIntegralDigits = 2;

	Args.Add(TEXT("Hours"), FText::AsNumber(Hours, &Options));
	Args.Add(TEXT("Minutes"), FText::AsNumber(Mins, &Options));
	Args.Add(TEXT("Seconds"), FText::AsNumber(Seconds, &Options));

	return FText::Format( NSLOCTEXT("SUWindowsMidGame","ClockFormat", "{Hours}:{Minutes}:{Seconds}"),Args);
}


void SUInGameMenu::OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	BuildOnlineBar();
}


void SUInGameMenu::OnMenuOpened()
{
	if (!PlayerOnlineStatusChangedDelegate.IsValid())
	{
		PlayerOnlineStatusChangedDelegate = PlayerOwner->RegisterPlayerOnlineStatusChangedDelegate(FPlayerOnlineStatusChanged::FDelegate::CreateSP(this, &SUInGameMenu::OwnerLoginStatusChanged));
	}

	SUWindowsDesktop::OnMenuOpened();
	OnInfo();
}

void SUInGameMenu::OnMenuClosed()
{
	PlayerOwner->RemovePlayerOnlineStatusChangedDelegate(PlayerOnlineStatusChangedDelegate);
	PlayerOnlineStatusChangedDelegate.Reset();

	SUWindowsDesktop::OnMenuClosed();

}

/****************************** [ Build Sub Menus ] *****************************************/

void SUInGameMenu::BuildTopBar()
{
	if (TopOverlay.IsValid())
	{
		TopOverlay->AddSlot()
		[
			SNew(SBox)
			.HeightOverride(32)
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush(PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Bar.Top")))
			]
		];
	}
}

/** Subclass me */
void SUInGameMenu::BuildDesktop()
{
}

void SUInGameMenu::BuildBottomBar()
{
	if (BottomOverlay.IsValid())
	{
		BottomOverlay->AddSlot()
		[
			SNew(SBox)
			.HeightOverride(58)
			[
				SNew(SOverlay)

				// First the background

				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush(PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Bar")))
				]

				// Now the actual menu

				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()		// This panel is just for alignment :(
					.AutoHeight()
					.HAlign(HAlign_Left)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()		// This is the Buffer at the Top
						.AutoHeight()
						.HAlign(HAlign_Fill)
						[
							SNew(SBox)
							.HeightOverride(7)
						]
						+ SVerticalBox::Slot()		// This is the Buffer at the Top
						.AutoHeight()
						.HAlign(HAlign_Fill)
						[
							// Create a space on the bottom bar for the Online Presence overlay
							SNew(SBox)
							.HeightOverride(51)
							[
								SAssignNew(OnlineOverlay, SOverlay)
							]
						]
					]
				]

				+SOverlay::Slot()
				[

					SNew(SVerticalBox)
					+ SVerticalBox::Slot()		// This panel is just for alignment :(
					.AutoHeight()
					.HAlign(HAlign_Right)
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
						.HAlign(HAlign_Fill)
						[
							SNew(SBox)
							.HeightOverride(51)
							[
								SAssignNew(MenuBar, SHorizontalBox)
							]
						]
					]
				]
			]
		];
	}

	// Build the default Menu and Online bars

	BuildMenuBar();
	BuildOnlineBar();

}

TSharedRef<SWidget> SUInGameMenu::BuildMenuBar()
{

	if (MenuBar.IsValid())
	{
		MenuBar->ClearChildren();
		BuildExitMatchSubMenu();
		BuildTeamSubMenu();
		BuildServerBrowserSubMenu();
		BuildOptionsSubMenu();
		BuildInfoSubMenu();
	}

	return MenuBar.ToSharedRef();
}

void SUInGameMenu::BuildInfoSubMenu()
{
	MenuBar->AddSlot()
		.AutoWidth()
		.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SUInGameMenu::OnInfo)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Generic", "Info", "INFO").ToString())
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
			]
		];
}

void SUInGameMenu::BuildPlaySubMenu()
{
	MenuBar->AddSlot()
		.AutoWidth()
		.Padding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SUInGameMenu::Play)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Gerneric","Play","PLAY"))
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
			]

		];
}


void SUInGameMenu::BuildTeamSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	AUTGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	bool bTeamGame = (GS != NULL && GS->bTeamGame);

	if (bTeamGame)
	{
		SAssignNew(DropDownButton, SComboButton)
		.MenuPlacement(MenuPlacement_AboveAnchor)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Teams", "TEAMS").ToString())
			.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
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

						if (PS->Team->GetTeamNum() != 0)
						{
							(*Menu).AddSlot()
							.AutoHeight()
							[
								SNew(SButton)
								.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
								.ContentPadding(FMargin(10.0f, 5.0f))
								.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Teams_SwitchToRed", "Switch to Red").ToString())
								.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle.AltTeam"))
								.OnClicked(this, &SUInGameMenu::OnChangeTeam, 0, DropDownButton)
							];
						}

						if (PS->Team->GetTeamNum() != 1)
						{
							(*Menu).AddSlot()
							.AutoHeight()
							[
								SNew(SButton)
								.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
								.ContentPadding(FMargin(10.0f, 5.0f))
								.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Teams_SwitchToBlue", "Switch to Blue").ToString())
								.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle.AltTeam"))
								.OnClicked(this, &SUInGameMenu::OnChangeTeam, 1, DropDownButton)
							];
						}


						if (PS->Team->GetTeamNum() != 255)
						{
							(*Menu).AddSlot()
							.AutoHeight()
							[
								SNew(SButton)
								.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
								.ContentPadding(FMargin(10.0f, 5.0f))
								.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Teams_SwitchToSpectate", "Switch to Spectator").ToString())
								.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
								.OnClicked(this, &SUInGameMenu::OnChangeTeam, 255, DropDownButton)
							];
						}
					}
				}
			}
			DropDownButton->SetMenuContent(Menu.ToSharedRef());
		}

		MenuBar->AddSlot()
			.AutoWidth()
			.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			.HAlign(HAlign_Right)
			[
				DropDownButton.ToSharedRef()
			];


	}
	else
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		bool bSpectator = (PS != NULL && PS->bIsSpectator);

		if (bSpectator)
		{
			MenuBar->AddSlot()
				.AutoWidth()
				.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Fill)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.OnClicked(this, &SUInGameMenu::OnChangeTeam, 0, DropDownButton)
					.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Teams_PLAY", "PLAY").ToString())
						.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
					]
				];
		}
		else
		{
			MenuBar->AddSlot()
				.AutoWidth()
				.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Fill)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.OnClicked(this, &SUInGameMenu::OnChangeTeam, 0, DropDownButton)
					.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Teams_Spectate", "SPECTATE").ToString())
						.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
					]
				];
		}
	}
}

void SUInGameMenu::BuildServerBrowserSubMenu()
{
	MenuBar->AddSlot()
		.AutoWidth()
		.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SUInGameMenu::OnServerBrowser)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Teams_ServerBrowser", "SERVER BROWSER").ToString())
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
			]

		];
}

void SUInGameMenu::BuildOptionsSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
		.HasDownArrow(false)
		.MenuPlacement(MenuPlacement_AboveAnchor)
		.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options", "OPTIONS").ToString())
			.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
		];

	DropDownButton->SetMenuContent
		(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_PlayerSettings", "Player Settings").ToString())
			.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
			.OnClicked(this, &SUInGameMenu::OpenSettingsDialog, DropDownButton, SettingsDialogs::SettingPlayer)
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_WeaponSettings", "Weapon Settings").ToString())
			.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
			.OnClicked(this, &SUInGameMenu::OpenSettingsDialog, DropDownButton, SettingsDialogs::SettingWeapon)
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_HUDSettings", "HUD Settings").ToString())
			.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
			.OnClicked(this, &SUInGameMenu::OpenSettingsDialog, DropDownButton, SettingsDialogs::SettingsHUD)
		]
		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_SystemSettings", "System Settings").ToString())
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
				.OnClicked(this, &SUInGameMenu::OpenSettingsDialog, DropDownButton, SettingsDialogs::SettingsSystem)
			] 
		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ControlSettings", "Control Settings").ToString())
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
				.OnClicked(this, &SUInGameMenu::OpenSettingsDialog, DropDownButton, SettingsDialogs::SettingsControls)
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
void SUInGameMenu::BuildExitMatchSubMenu()
{
	AUTGameState* UTGameState = GWorld->GetGameState<AUTGameState>();
	if (GWorld->GetNetMode() == NM_Standalone)
	{
		MenuBar->AddSlot()
			.AutoWidth()
			.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.OnClicked(this, &SUInGameMenu::ExitMatch)
				.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_ExitMatch", "EXIT MATCH"))
					.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
				]

			];
	}
	else if (UTGameState && UTGameState->bIsInstanceServer)
	{
		TSharedPtr<SComboButton> DropDownButton = NULL;

		SAssignNew(DropDownButton, SComboButton)
			.HasDownArrow(false)
			.MenuPlacement(MenuPlacement_AboveAnchor)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_ExitMatch", "EXIT MATCH"))
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
			];

		DropDownButton->SetMenuContent
			(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Disconnect", "Exit to Main Menu"))
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
				.OnClicked(this, &SUInGameMenu::Disconnect, DropDownButton)
			]
		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_ReturnToLobby", "Return to the Hub"))
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
				.OnClicked(this, &SUInGameMenu::ReturnToLobby, DropDownButton)
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
	else
	{
		TSharedPtr<SComboButton> DropDownButton = NULL;

		SAssignNew(DropDownButton, SComboButton)
			.HasDownArrow(false)
			.MenuPlacement(MenuPlacement_AboveAnchor)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_ExitMatch", "EXIT MATCH"))
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
			];

		DropDownButton->SetMenuContent
			(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Disconnect", "Exit to Main Menu"))
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
				.OnClicked(this, &SUInGameMenu::Disconnect, DropDownButton)
			]
		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
				.ContentPadding(FMargin(10.0f, 5.0f))
				.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Reconnect", "Reconnect to last Server"))
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
				.OnClicked(this, &SUInGameMenu::Reconnect, DropDownButton)
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
}

void SUInGameMenu::BuildOnlineBar()
{
	// Remove any existing online status information
	if (OnlineOverlay.IsValid())
	{
		OnlineOverlay->ClearChildren();
		if (PlayerOwner->IsLoggedIn())
		{
			OnlineOverlay->AddSlot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()		// This panel is just for alignment :(
				.AutoHeight()
				.HAlign(HAlign_Right)
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
					.HAlign(HAlign_Fill)
					.Padding(115.0f,0.0f,0.0f,0.0f)
					[
						SNew(SBox)
						.HeightOverride(51)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							[
								SNew(STextBlock)
								.Text(FText::FromString(PlayerOwner->GetNickname()))
								.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.TextStyle")
							]
							+SVerticalBox::Slot()
							[

								SNew(STextBlock)
								.Text(PlayerOwner->GetAccountSummary())
								.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MOTD.RulesText")

							]

						]
					]
				]
			];
		}
		else
		{
			OnlineOverlay->AddSlot()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Fill)
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.OnClicked(this, &SUInGameMenu::OnLogin)
						.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("Login", "NeedToLogin", "Click to log in").ToString())
							.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
						]
					]
			];

		}
	}

}


FReply SUInGameMenu::Disconnect(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	ExitMatch();
	return FReply::Handled();
}

FReply SUInGameMenu::Reconnect(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	ConsoleCommand("reconnect");
	CloseMenus();
	return FReply::Handled();
}


FReply SUInGameMenu::OpenSettingsDialog(TSharedPtr<SComboButton> MenuButton, FName SettingsToOpen)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	if (SettingsToOpen == SettingsDialogs::SettingsHUD)
	{
		CloseMenus();
		PlayerOwner->ShowHUDSettings();
		return FReply::Handled();
	}

	TSharedPtr<SUWDialog> Dialog;
	if (SettingsToOpen == SettingsDialogs::SettingPlayer) SAssignNew(Dialog, SUWPlayerSettingsDialog).PlayerOwner(PlayerOwner);
	else if (SettingsToOpen == SettingsDialogs::SettingWeapon) SAssignNew(Dialog, SUWWeaponConfigDialog).PlayerOwner(PlayerOwner);
	else if (SettingsToOpen == SettingsDialogs::SettingsSystem) SAssignNew(Dialog, SUWSystemSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop", "System", "System Settings"));
	else if (SettingsToOpen == SettingsDialogs::SettingsControls) SAssignNew(Dialog, SUWControlSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop","Controls","Control Settings"));

	if (Dialog.IsValid())
	{
		PlayerOwner->OpenDialog(Dialog.ToSharedRef());
	}
	return FReply::Handled();
}

FReply SUInGameMenu::Play()
{
	CloseMenus();
	return FReply::Handled();
}


FReply SUInGameMenu::ExitMatch()
{
	ConsoleCommand(TEXT("open UT-Entry?Game=/Script/UnrealTournament.UTMenuGameMode"));
	CloseMenus();
	return FReply::Handled();
}

FReply SUInGameMenu::ReturnToLobby(TSharedPtr<SComboButton> MenuButton)
{
	// Look to see if we have a server to return to.

	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	if (PlayerOwner->LastLobbyServerGUID != TEXT(""))
	{
		// Connect to that GUID
		AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
		if (PC)
		{
			PC->ConnectToServerViaGUID(PlayerOwner->LastLobbyServerGUID, false);
			CloseMenus();
			return FReply::Handled();
		}
	}
	return ExitMatch();
}

FReply SUInGameMenu::OnChangeTeam(int32 NewTeamIndex,TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	
	ConsoleCommand(FString::Printf(TEXT("changeteam %i"), NewTeamIndex));
	CloseMenus();
	return FReply::Handled();

}

FReply SUInGameMenu::OnServerBrowser()
{
	TSharedPtr<class SUWServerBrowser> Browser = PlayerOwner->GetServerBrowser();
	if (Browser.IsValid())
	{
		ActivatePanel(Browser);
	}
	return FReply::Handled();

}

FReply SUInGameMenu::OnInfo()
{
	BuildDesktop();
	return FReply::Handled();
}

FReply SUInGameMenu::OnLogin()
{
	PlayerOwner->LoginOnline(TEXT(""),TEXT(""));
	return FReply::Handled();
}

#endif