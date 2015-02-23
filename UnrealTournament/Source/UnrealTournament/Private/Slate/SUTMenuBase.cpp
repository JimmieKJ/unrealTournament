// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsStyle.h"
#include "SUTMenuBase.h"
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
#include "Panels/SUWServerBrowser.h"
#include "Panels/SUWStatsViewer.h"
#include "Panels/SUWCreditsPanel.h"

#if !UE_SERVER

void SUTMenuBase::CreateDesktop()
{
	bNeedsPlayerOptions = false;
	bNeedsWeaponOptions = false;
	bShowingFriends = false;
	TickCountDown = 0;
	
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
									BuildDefaultLeftMenuBar()
								]
								+SOverlay::Slot()
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Center)
								[
									BuildDefaultRightMenuBar()
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

	SetInitialPanel();
}

void SUTMenuBase::SetInitialPanel()
{
}


/****************************** [ Build Sub Menus ] *****************************************/

void SUTMenuBase::BuildLeftMenuBar() {}
void SUTMenuBase::BuildRightMenuBar() {}

TSharedRef<SWidget> SUTMenuBase::BuildDefaultLeftMenuBar()
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
			.OnClicked(this, &SUTMenuBase::OnShowHomePanel)
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

		if (ShouldShowBrowserIcon())
		{
			LeftMenuBar->AddSlot()
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button")
				.OnClicked(this, &SUTMenuBase::OnShowServerBrowserPanel)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(48)
						.HeightOverride(48)
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Browser"))
						]
					]
				]
			];
		}


		BuildLeftMenuBar();
	}

	return LeftMenuBar.ToSharedRef();
}

FText SUTMenuBase::GetBrowserButtonText() const
{
	return PlayerOwner->GetWorld()->GetNetMode() == ENetMode::NM_Standalone ? NSLOCTEXT("SUWindowsDesktop","MenuBar_HUBS","PLAY ONLINE") : NSLOCTEXT("SUWindowsDesktop","MenuBar_Browser","SERVER BROWSER");
}

TSharedRef<SWidget> SUTMenuBase::BuildDefaultRightMenuBar()
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

		BuildRightMenuBar();

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

		TSharedPtr<SComboButton> ExitButton = NULL;

		RightMenuBar->AddSlot()
		.Padding(0.0f,0.0f,5.0f,0.0f)
		.AutoWidth()
		[

			SAssignNew(ExitButton, SComboButton)
			.HasDownArrow(false)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Right")
			.ButtonContent()
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

		if (ExitButton.IsValid())
		{
			TSharedPtr<SVerticalBox> MenuSpace;
			SAssignNew(MenuSpace, SVerticalBox);
			if (MenuSpace.IsValid())
			{
				// Allow children to place menu options here....
				BuildExitMenu(ExitButton, MenuSpace);

				if (MenuSpace->NumSlots() > 0)
				{
					MenuSpace->AddSlot()
					.AutoHeight()
					[
						SNew(SBox)
						.HeightOverride(16)
						[
							SNew(SButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button.Empty")
						]
					];

					MenuSpace->AddSlot()
					.AutoHeight()
					[
						SNew(SBox)
						.HeightOverride(16)
						[
							SNew(SButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button.Spacer")
						]
					];
				}

				MenuSpace->AddSlot()
				.AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(NSLOCTEXT("SUTMenuBase", "MenuBar_Exit_QuitGame", "QUIT THE GAME").ToString())
					.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
					.OnClicked(this, &SUTMenuBase::OnMenuConsoleCommand, FString(TEXT("quit")), ExitButton)
				];
			}

			ExitButton->SetMenuContent( MenuSpace.ToSharedRef());
		}



	}


	return RightMenuBar.ToSharedRef();
}

TSharedRef<SWidget> SUTMenuBase::BuildOptionsSubMenu()
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
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ClearCloud", "Clear Game Settings").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUTMenuBase::ClearCloud, DropDownButton)
		]
	);

	MenuButtons.Add(DropDownButton);
	return DropDownButton.ToSharedRef();

}

TSharedRef<SWidget> SUTMenuBase::BuildAboutSubMenu()
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
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_TPSReport", "Third Party Software").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUTMenuBase::OpenTPSReport, DropDownButton)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_Credits", "Credits").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUTMenuBase::OpenCredits, DropDownButton)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_UTSite", "UnrealTournament.com").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUTMenuBase::OnMenuHTTPButton, FString(TEXT("http://www.unrealtournament.com/")),DropDownButton)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_UTForums", "Forums").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUTMenuBase::OnMenuHTTPButton, FString(TEXT("http://forums.unrealtournament.com/")), DropDownButton)
		]

#if UE_BUILD_DEBUG
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_WR", "Widget Reflector").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUTMenuBase::OnMenuConsoleCommand, FString(TEXT("widgetreflector")), DropDownButton)
		]
#endif

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::Format(NSLOCTEXT("SUWindowsDesktop", "MenuBar_NetVersion", "Network Version: {Ver}"), FText::FromString(FString::Printf(TEXT("%i"), GetDefault<UUTGameEngine>()->GameNetworkVersion))).ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.Version.TextStyle")
		]

	);

	MenuButtons.Add(DropDownButton);
	return DropDownButton.ToSharedRef();
}

void SUTMenuBase::BuildExitMenu(TSharedPtr<SComboButton> ExitButton, TSharedPtr<SVerticalBox> MenuSpace)
{
}


FReply SUTMenuBase::OpenPlayerSettings(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	if (TickCountDown <= 0)
	{
		PlayerOwner->ShowContentLoadingMessage();
		bNeedsPlayerOptions = true;
		TickCountDown = 3;
	}

	return FReply::Handled();
}

FReply SUTMenuBase::OpenWeaponSettings(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	if (TickCountDown <= 0)
	{
		PlayerOwner->ShowContentLoadingMessage();
		bNeedsWeaponOptions = true;
		TickCountDown = 3;
	}

	return FReply::Handled();
}

FReply SUTMenuBase::OpenSystemSettings(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	PlayerOwner->OpenDialog(SNew(SUWSystemSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop","System","System Settings")));
	return FReply::Handled();
}

FReply SUTMenuBase::OpenControlSettings(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	PlayerOwner->OpenDialog(SNew(SUWControlSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop","Controls","Control Settings")));
	return FReply::Handled();
}

FReply SUTMenuBase::ClearCloud(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	if (PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->ClearProfileSettings();				
	}
	else
	{
		PlayerOwner->ShowMessage(NSLOCTEXT("SUWindowsMainMenu","NotLoggedInTitle","Not Logged In"), NSLOCTEXT("SuWindowsMainMenu","NoLoggedInMsg","You need to be logged in to clear your cloud settings!"), UTDIALOG_BUTTON_OK);
	}
	return FReply::Handled();
}


FReply SUTMenuBase::OpenTPSReport(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	PlayerOwner->OpenDialog(
							SNew(SUWMessageBox)
							.PlayerOwner(PlayerOwner)
							.DialogSize(FVector2D(0.7, 0.8))
							.bDialogSizeIsRelative(true)
							.DialogTitle(NSLOCTEXT("SUWindowsDesktop", "TPSReportTitle", "Third Party Software"))
							.MessageText(NSLOCTEXT("SUWindowsDesktop", "TPSReportText", "TPSText"))
							//.MessageTextStyleName("UWindows.Standard.Dialog.TextStyle.Legal")
							.MessageTextStyleName("UT.Common.NormalText")
							.ButtonMask(UTDIALOG_BUTTON_OK)
							);
	return FReply::Handled();
}

FReply SUTMenuBase::OpenCredits(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	TSharedPtr<class SUWCreditsPanel> CreditsPanel = PlayerOwner->GetCreditsPanel();
	if (CreditsPanel.IsValid())
	{
		ActivatePanel(CreditsPanel);
	}
	return FReply::Handled();
}

FReply SUTMenuBase::OnMenuHTTPButton(FString URL,TSharedPtr<SComboButton> MenuButton)
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


FReply SUTMenuBase::OnShowStatsViewer()
{
	TSharedPtr<class SUWStatsViewer> StatsViewer = PlayerOwner->GetStatsViewer();
	if (StatsViewer.IsValid())
	{
		ActivatePanel(StatsViewer);
	}
	return FReply::Handled();
}

FReply SUTMenuBase::OnCloseClicked()
{
	return FReply::Handled();
}

TSharedRef<SWidget> SUTMenuBase::BuildOnlinePresence()
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
				.OnClicked(this, &SUTMenuBase::OnOnlineClick)		
				[
					SNew(SHorizontalBox)
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
				.OnClicked(this, &SUTMenuBase::OnShowStatsViewer)
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
					.OnClicked(this, &SUTMenuBase::ToggleFriendsAndFamily)
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
		.OnClicked(this, &SUTMenuBase::OnOnlineClick)		
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

void SUTMenuBase::OnOwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	// Rebuilds the right menu bar
	BuildDefaultRightMenuBar();
}

FReply SUTMenuBase::OnOnlineClick()
{
	PlayerOwner->LoginOnline(TEXT(""),TEXT(""),false);
	return FReply::Handled();
}

FReply SUTMenuBase::OnShowServerBrowser(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	return OnShowServerBrowserPanel();
}


FReply SUTMenuBase::OnShowServerBrowserPanel()
{

	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline(TEXT(""), TEXT(""));
		return FReply::Handled();
	}
	

	TSharedPtr<class SUWServerBrowser> Browser = PlayerOwner->GetServerBrowser();
	if (Browser.IsValid())
	{
		ActivatePanel(Browser);
	}

	return FReply::Handled();
}

FReply SUTMenuBase::ToggleFriendsAndFamily()
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

void SUTMenuBase::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// NOTE: TickCountDown is in frames, not time.  We have to delay 3 frames before opening a blocking menu to insure the
	// meesage gets displayed.

	if (TickCountDown > 0)
	{
		TickCountDown--;

		if (TickCountDown <= 0)
		{
			OpenDelayedMenu();
		}
	}
}

void SUTMenuBase::OpenDelayedMenu()
{
	if (bNeedsPlayerOptions)
	{
		PlayerOwner->OpenDialog(SNew(SUWPlayerSettingsDialog).PlayerOwner(PlayerOwner));
		bNeedsPlayerOptions = false;
		PlayerOwner->HideContentLoadingMessage();
	}
	else if (bNeedsWeaponOptions)
	{
		PlayerOwner->OpenDialog(SNew(SUWWeaponConfigDialog).PlayerOwner(PlayerOwner));
		bNeedsWeaponOptions = false;
		PlayerOwner->HideContentLoadingMessage();
	}
}

FReply SUTMenuBase::OnShowHomePanel()
{
	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
	else if (ActivePanel.IsValid())
	{
		DeactivatePanel(ActivePanel);
	}

	return FReply::Handled();
}


#endif