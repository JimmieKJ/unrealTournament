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
#include "Panels/SUWServerBrowser.h"
#include "UTLobbyGameState.h"
#include "Panels/SUMidGameInfoPanel.h"

namespace SettingsDialogs
{
	const FName SettingPlayer = FName(TEXT("SettingsPlayern"));
	const FName SettingsSystem = FName(TEXT("SettingsSystem"));
	const FName SettingsControls = FName(TEXT("SettingsControls"));
}

#if !UE_SERVER
void SUWindowsMidGame::CreateDesktop()
{
	MenuBar = NULL;
	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[

			SNew(SOverlay)

			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.DarkBackground"))
			]

			+SOverlay::Slot()
			[

				SNew(SVerticalBox)

				+ SVerticalBox::Slot()		// This is the Buffer at the Top
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SBox)
						.HeightOverride(32)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush(PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Bar.Top")))
						]
					]
					+SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.HAlign(HAlign_Fill)
						.Padding(0,3,0,0)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(5,0,0,0)
							[
								SNew(SBox)
								.WidthOverride(100)
								.HAlign(HAlign_Left)
								[
									SAssignNew(ClockText, STextBlock)
									.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Status.TextStyle"))
								]
							]

							+SHorizontalBox::Slot()
							.Padding(0,0,5,0)
							.HAlign(HAlign_Right)
							[
								SAssignNew(StatusText, STextBlock)
								.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Status.TextStyle"))
							]
						]
					]
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
					SNew(SBox)
					.HeightOverride(58)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush(PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Bar")))
						]

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
										BuildMenuBar()
									]
								]
							]
						]
					]
				]
			]
			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.VAlign(VAlign_Bottom)
				.Padding(5.0f,0.0f,0.0f,5.0f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						SNew(SBox)
						.HeightOverride(128)
						.WidthOverride(102)
						[
							SAssignNew(PortraitImage, SImage)
							.Visibility(EVisibility::Hidden)
						]
					]
				]
			]
		];

	UpdateOnlineState();

}

void SUWindowsMidGame::UpdateOnlineState()
{
	// Remove any existing online status information
	OnlineOverlay->ClearChildren();

	if (PlayerOwner->IsLoggedIn())
	{
		PortraitImage->SetVisibility(EVisibility::Visible);
		PortraitImage->SetImage(SUWindowsStyle::Get().GetBrush("Testing.TestPortrait"));

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
		PortraitImage->SetVisibility(EVisibility::Hidden);
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
					.OnClicked(this, &SUWindowsMidGame::OnLogin)
					.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Login", "NeedToLogin", "Click to Login").ToString())
						.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
					]
				]
		];

	}

}

FText SUWindowsMidGame::ConvertTime(int Seconds)
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


void SUWindowsMidGame::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SUWindowsDesktop::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	AUTGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();

	if (GS != NULL)
	{
		FText NewText;

		if (GS->bTeamGame)
		{
			int32 Players[2];
			int32 Specs = 0;

			Players[0] = 0;
			Players[1] = 0;

			for (int32 i=0;i<GS->PlayerArray.Num();i++)
			{
				if (GS->PlayerArray[i] != NULL)
				{
					AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
					if (PS != NULL)
					{
						if (PS->bIsSpectator)
						{
							Specs++;
						}
						else
						{
							Players[FMath::Clamp<int32>(PS->GetTeamNum(),0,1)]++;
						}
					}
				}
			}

			NewText = FText::Format(NSLOCTEXT("SUWindowsMidGame","TeamStatus","Red: {0}  Blue: {1}  Spectators {2}"), FText::AsNumber(Players[0]), FText::AsNumber(Players[1]), FText::AsNumber(Specs));
		}
		else
		{
			int32 Players = 0;
			int32 Specs = 0;
			for (int32 i=0;i<GS->PlayerArray.Num();i++)
			{
				if (GS->PlayerArray[i] != NULL)
				{
					AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
					if (PS != NULL)
					{
						if (PS->bIsSpectator)
						{
							Specs++;
						}
						else 
						{
							Players++;
						}
					}
				}
			}

			NewText = FText::Format(NSLOCTEXT("SUWindowsMidGame","Status","Players: {0} Spectators {1}"), FText::AsNumber(Players), FText::AsNumber(Specs));
		}

		ClockText->SetText(ConvertTime(GS->ElapsedTime));
		StatusText->SetText(NewText);
	}

}

void SUWindowsMidGame::OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	UpdateOnlineState();
}


void SUWindowsMidGame::OnMenuOpened()
{
	PlayerOnlineStatusChangedDelegate.BindSP(this, &SUWindowsMidGame::OwnerLoginStatusChanged);
	PlayerOwner->AddPlayerLoginStatusChangedDelegate(PlayerOnlineStatusChangedDelegate);

	SUWindowsDesktop::OnMenuOpened();
	OnInfo();
}

void SUWindowsMidGame::OnMenuClosed()
{
	PlayerOwner->ClearPlayerLoginStatusChangedDelegate(PlayerOnlineStatusChangedDelegate);
	SUWindowsDesktop::OnMenuClosed();

}

/****************************** [ Build Sub Menus ] *****************************************/

TSharedRef<SWidget> SUWindowsMidGame::BuildMenuBar()
{
	SAssignNew(MenuBar, SHorizontalBox);
	if (MenuBar.IsValid())
	{

		MenuBar->AddSlot()
			.AutoWidth()
			.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.OnClicked(this, &SUWindowsMidGame::OnInfo)
				.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Generic", "Info", "INFO").ToString())
					.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
				]
			];

		BuildExitMatchSubMenu();
		BuildTeamSubMenu();
		BuildServerBrowserSubMenu();
		BuildOptionsSubMenu();

		MenuBar->AddSlot()
			.AutoWidth()
			.Padding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.OnClicked(this, &SUWindowsMidGame::Play)
				.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Gerneric","Play","PLAY"))
					.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
				]

			];


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
		.MenuPlacement(MenuPlacement_AboveAnchor)
		.Method(SMenuAnchor::UseCurrentWindow)
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
								.OnClicked(this, &SUWindowsMidGame::OnChangeTeam, 0, DropDownButton)
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
								.OnClicked(this, &SUWindowsMidGame::OnChangeTeam, 1, DropDownButton)
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
								.OnClicked(this, &SUWindowsMidGame::OnChangeTeam, 255, DropDownButton)
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
					.OnClicked(this, &SUWindowsMidGame::OnChangeTeam, 0, DropDownButton)
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
					.OnClicked(this, &SUWindowsMidGame::OnChangeTeam, 0, DropDownButton)
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

void SUWindowsMidGame::BuildServerBrowserSubMenu()
{
	MenuBar->AddSlot()
		.AutoWidth()
		.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SUWindowsMidGame::OnServerBrowser)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Teams_ServerBrowser", "SERVER BROWSER").ToString())
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
			]

		];
}

void SUWindowsMidGame::BuildOptionsSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
		.HasDownArrow(false)
		.MenuPlacement(MenuPlacement_AboveAnchor)
		.Method(SMenuAnchor::UseCurrentWindow)
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
			.OnClicked(this, &SUWindowsMidGame::OpenSettingsDialog, DropDownButton, SettingsDialogs::SettingPlayer)
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_SystemSettings", "System Settings").ToString())
			.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
			.OnClicked(this, &SUWindowsMidGame::OpenSettingsDialog, DropDownButton, SettingsDialogs::SettingsSystem)
		] 
	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.MenuList"))
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ControlSettings", "Control Settings").ToString())
			.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.SubMenu.TextStyle"))
			.OnClicked(this, &SUWindowsMidGame::OpenSettingsDialog, DropDownButton, SettingsDialogs::SettingsControls)
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
	FText MenuText = PlayerOwner->GetWorld()->GetNetMode() == NM_Standalone ?
		NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_ExitMatch", "EXIT MATCH") :
		NSLOCTEXT("SUWindowsMidGameMenu", "MenuBar_Disconnect", "DISCONNECT");

	MenuBar->AddSlot()
		.AutoWidth()
		.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SUWindowsMidGame::ExitMatch)
			.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
			[
				SNew(STextBlock)
				.Text(MenuText.ToString())
				.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
			]

		];
}

FReply SUWindowsMidGame::OpenSettingsDialog(TSharedPtr<SComboButton> MenuButton, FName SettingsToOpen)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	TSharedPtr<SUWDialog> Dialog;
	if (SettingsToOpen == SettingsDialogs::SettingPlayer) SAssignNew(Dialog, SUWPlayerSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop","PlayerSettings","Player Settings"));
	else if (SettingsToOpen == SettingsDialogs::SettingsSystem) SAssignNew(Dialog, SUWSystemSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop","System","System Settings"));
	else if (SettingsToOpen == SettingsDialogs::SettingsControls) SAssignNew(Dialog, SUWControlSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop","Controls","Control Settings"));

	if (Dialog.IsValid())
	{
		PlayerOwner->OpenDialog(Dialog.ToSharedRef());
	}
	return FReply::Handled();
}

FReply SUWindowsMidGame::Play()
{
	CloseMenus();
	return FReply::Handled();
}


FReply SUWindowsMidGame::ExitMatch()
{
	ConsoleCommand(TEXT("open UT-Entry?Game=/Script/UnrealTournament.UTMenuGameMode"));
	CloseMenus();
	return FReply::Handled();
}

FReply SUWindowsMidGame::OnChangeTeam(int32 NewTeamIndex,TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	
	ConsoleCommand(FString::Printf(TEXT("changeteam %i"), NewTeamIndex));
	CloseMenus();
	return FReply::Handled();

}

FReply SUWindowsMidGame::OnServerBrowser()
{
	TSharedPtr<class SUWServerBrowser> Browser = PlayerOwner->GetServerBrowser();
	if (Browser.IsValid())
	{
		ActivatePanel(Browser);
	}
	return FReply::Handled();

}

FReply SUWindowsMidGame::OnInfo()
{
	if (!InfoPanel.IsValid())
	{
		SAssignNew(InfoPanel, SUMidGameInfoPanel).PlayerOwner(PlayerOwner);
	}
	ActivatePanel(InfoPanel);
	return FReply::Handled();
}

FReply SUWindowsMidGame::OnLogin()
{
	PlayerOwner->LoginOnline(TEXT(""),TEXT(""));
	return FReply::Handled();
}

#endif