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
#include "Panels/SUWServerBrowser.h"
#include "Panels/SUWStatsViewer.h"
#include "Panels/SUWCreditsPanel.h"

#if !UE_SERVER

void SUWindowsMainMenu::CreateDesktop()
{

	bNeedToShowGamePanel = false;
	bShowingFriends = false;
	LeftMenuBar = NULL;
	RightMenuBar = NULL;
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
				.HeightOverride(64)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.TileBar"))
						]
					]
					+SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBox)
							.HeightOverride(56)
							[
								// Left Menu
								SNew(SOverlay)
								+SOverlay::Slot()
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									BuildLeftMenuBar()
								]
								+SOverlay::Slot()
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Center)
								[
									BuildRightMenuBar()
								]
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBox)
							.HeightOverride(8)
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
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SAssignNew(Desktop, SOverlay)
				]
			]
		];


	SAssignNew(HomePanel, SUHomePanel)
		.PlayerOwner(PlayerOwner);

	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
}

/****************************** [ Build Sub Menus ] *****************************************/

TSharedRef<SWidget> SUWindowsMainMenu::BuildLeftMenuBar()
{
	SAssignNew(LeftMenuBar, SHorizontalBox);
	if (LeftMenuBar.IsValid())
	{
		LeftMenuBar->AddSlot()
		.Padding(5.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button")
			.OnClicked(this, &SUWindowsMainMenu::OnShowHomePanel)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(48)
					.HeightOverride(48)
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Home"))
					]
				]
			]
		];

		LeftMenuBar->AddSlot()
		.Padding(5.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Left")
			.OnClicked(this, &SUWindowsMainMenu::OnShowGamePanel)
			.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_CreateGame","NEW GAME").ToString())
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
				]
			]
		];

		LeftMenuBar->AddSlot()
		.Padding(5.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Left")
			.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_QuickMatch","QUICK MATCH").ToString())
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
				]
			]
		];

		LeftMenuBar->AddSlot()
		.Padding(5.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Left")
			.OnClicked(this, &SUWindowsMainMenu::OnShowServerBrowserPanel)
			.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_HUBS","PLAY ONLINE").ToString())
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
				]
			]
		];
	}

	return LeftMenuBar.ToSharedRef();
}

TSharedRef<SWidget> SUWindowsMainMenu::BuildRightMenuBar()
{
	// Build the Right Menu Bar
	if (!RightMenuBar.IsValid())
	{
		SAssignNew(RightMenuBar, SHorizontalBox);
	}
	else
	{
		RightMenuBar->ClearChildren();
	}

	if (RightMenuBar.IsValid())
	{
		RightMenuBar->AddSlot()
		.Padding(0.0f,0.0f,5.0f,0.0f)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			BuildOnlinePresence()
		];


		RightMenuBar->AddSlot()
		.Padding(0.0f,0.0f,5.0f,0.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(48)
			.HeightOverride(48)
			[

				BuildOptionsSubMenu()
			]
		];

		RightMenuBar->AddSlot()
		.Padding(0.0f,0.0f,35.0f,0.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(48)
			.HeightOverride(48)
			[

				BuildAboutSubMenu()
			]
		];


		RightMenuBar->AddSlot()
		.Padding(0.0f,0.0f,5.0f,0.0f)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button")
			.OnClicked(this, &SUWindowsMainMenu::OnLeaveMatch)
			[
				SNew(SBox)
				.WidthOverride(48)
				.HeightOverride(48)
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Exit"))
				]
			]
		];


	}

	return RightMenuBar.ToSharedRef();
}

TSharedRef<SWidget> SUWindowsMainMenu::BuildOptionsSubMenu()
{
	
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Right")
		.ButtonContent()
		[
			SNew(SImage)
			.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Settings"))
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

	MenuButtons.Add(DropDownButton);
	return DropDownButton.ToSharedRef();

}

TSharedRef<SWidget> SUWindowsMainMenu::BuildAboutSubMenu()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
	.HasDownArrow(false)
	.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Right")
	.ButtonContent()
	[
		SNew(SImage)
		.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.About"))
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

#if UE_BUILD_DEBUG
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
#endif

	);

	MenuButtons.Add(DropDownButton);
	return DropDownButton.ToSharedRef();
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

FReply SUWindowsMainMenu::OnShowStatsViewer()
{
	TSharedPtr<class SUWStatsViewer> StatsViewer = PlayerOwner->GetStatsViewer();
	if (StatsViewer.IsValid())
	{
		ActivatePanel(StatsViewer);
	}
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnLeaveMatch()
{
	PlayerOwner->HideMenu();
	ConsoleCommand(TEXT("quit"));
	return FReply::Handled();
}

TSharedRef<SWidget> SUWindowsMainMenu::BuildOnlinePresence()
{
	if ( PlayerOwner->IsLoggedIn() )
	{
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Right")
				.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
				.OnClicked(this, &SUWindowsMainMenu::OnOnlineClick)		
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(48)
							.HeightOverride(48)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.SignOut"))
							]
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5.0f,0.0f,25.0f,0.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(*PlayerOwner->GetOnlinePlayerNickname()))
						.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
					]
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Right")
				.OnClicked(this, &SUWindowsMainMenu::OnShowStatsViewer)
				.ToolTipText(NSLOCTEXT("ToolTips","TPMyStats","Show stats for this player."))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(48)
						.HeightOverride(48)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Stats"))
						]
					]
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SMenuAnchor)
				.Method(EPopupMethod::UseCurrentWindow)
				[
					SNew(SButton)
					.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Right")
					.OnClicked(this, &SUWindowsMainMenu::ToggleFriendsAndFamily)
					.ToolTipText(NSLOCTEXT("ToolTips","TPFriends","Show / Hide your friends list."))
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(48)
							.HeightOverride(48)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Online"))
							]
						]
					]

				]
			];
	}
	else
	{
		return 	SNew(SButton)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Right")
		.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
		.OnClicked(this, &SUWindowsMainMenu::OnOnlineClick)		
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(48)
				.HeightOverride(48)
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.SignIn"))
				]

			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_SignIn","Sign In").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
			]
		];
	}
}

void SUWindowsMainMenu::OnOwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	// Rebuilds the right menu bar
	BuildRightMenuBar();
}

FReply SUWindowsMainMenu::OnOnlineClick()
{
	if (PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->Logout();	
		OnShowHomePanel();
	}
	else
	{
		PlayerOwner->LoginOnline(TEXT(""),TEXT(""),false);
	}

	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnShowHomePanel()
{
	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
	return FReply::Handled();
}

void SUWindowsMainMenu::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bNeedToShowGamePanel)
	{
		TickCountDown--;

		if (TickCountDown <= 0)
		{
			ShowGamePanel();
			bNeedToShowGamePanel = false;
		}
	}
}


FReply SUWindowsMainMenu::OnShowGamePanel()
{
	PlayerOwner->ShowContentLoadingMessage();
	bNeedToShowGamePanel = true;
	TickCountDown = 3;
	return FReply::Handled();
}

void SUWindowsMainMenu::ShowGamePanel()
{
	if (!GamePanel.IsValid())
	{
		SAssignNew(GamePanel, SUWCreateGamePanel)
			.PlayerOwner(PlayerOwner);
	}

	if (GamePanel.IsValid())
	{
		ActivatePanel(GamePanel);
	}

	PlayerOwner->HideContentLoadingMessage();
	
}

FReply SUWindowsMainMenu::OnShowServerBrowserPanel()
{
	TSharedPtr<class SUWServerBrowser> Browser = PlayerOwner->GetServerBrowser();
	if (Browser.IsValid())
	{
		ActivatePanel(Browser);
	}

	return FReply::Handled();
}

FReply SUWindowsMainMenu::ToggleFriendsAndFamily()
{
	if (bShowingFriends)
	{
		Desktop->RemoveSlot(200);
		bShowingFriends = false;
	}
	else
	{
		TSharedPtr<SWidget> Popup = PlayerOwner->GetFriendsPopup();
		if (Popup.IsValid())
		{
			Desktop->AddSlot(200)
				[
					Popup.ToSharedRef()
				];
			bShowingFriends = true;
		}
	}


	return FReply::Handled();
}
#endif