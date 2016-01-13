// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWindowsDesktop.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "../Base/SUTMenuBase.h"
#include "../Base/SUTDialogBase.h"
#include "../Dialogs/SUTSystemSettingsDialog.h"
#include "../Dialogs/SUTPlayerSettingsDialog.h"
#include "../Dialogs/SUTSocialSettingsDialog.h"
#include "../Dialogs/SUTWeaponConfigDialog.h"
#include "../Dialogs/SUTControlSettingsDialog.h"
#include "../Dialogs/SUTInputBoxDialog.h"
#include "../Dialogs/SUTMessageBoxDialog.h"
#include "../Widgets/SUTScaleBox.h"
#include "SUTFriendsPopupWindow.h"
#include "UTGameEngine.h"
#include "SUTServerBrowserPanel.h"
#include "SUTStatsViewerPanel.h"
#include "SUTCreditsPanel.h"
#include "UnrealNetwork.h"
#include "SUTProfileItemsDialog.h"
#include "SUTQuickPickDialog.h"

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
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				BuildBackground()
			]
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				[
					SNew(SBox)
					.HeightOverride(64)
					[
						SNew(SBorder)
						.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBox)
								.HeightOverride(56)
								[
									// Left Menu
									SNew(SOverlay)
									+ SOverlay::Slot()
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Center)
									[
										BuildDefaultLeftMenuBar()
									]
									+ SOverlay::Slot()
										.HAlign(HAlign_Right)
										.VAlign(VAlign_Center)
										[
											BuildDefaultRightMenuBar()
										]
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
						+ SOverlay::Slot()
						[
							SAssignNew(Desktop, SOverlay)
						]
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
			SNew(SUTButton)
			.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
			.OnClicked(this, &SUTMenuBase::OnShowHomePanel)
			.Visibility(this, &SUTMenuBase::GetBackVis)
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
						.Image(SUTStyle::Get().GetBrush("UT.Icon.Back"))
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
				SNew(SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
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
							.Image(SUTStyle::Get().GetBrush("UT.Icon.Browser"))
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
	return PlayerOwner->GetWorld()->GetNetMode() == ENetMode::NM_Standalone ? NSLOCTEXT("SUWindowsDesktop","MenuBar_HUBS","Play Online") : NSLOCTEXT("SUWindowsDesktop","MenuBar_Browser","Server Browser");
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
		[
			BuildAboutSubMenu()
		];

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
			BuildOptionsSubMenu()
		];

		RightMenuBar->AddSlot()
		.Padding(0.0f,0.0f,5.0f,0.0f)
		.AutoWidth()
		[
			SNew(SUTButton)
			.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
			.OnClicked(this, &SUTMenuBase::MinimizeClicked)
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
						.Image(SUTStyle::Get().GetBrush("UT.Icon.Minimize"))
					]
				]
			]
		];

		RightMenuBar->AddSlot()
		.Padding(0.0f,0.0f,5.0f,0.0f)
		.AutoWidth()
		[
			SNew(SUTButton)
			.OnClicked(this, &SUTMenuBase::ToggleFullscreenClicked)
			.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
			[
				SNew(SBox)
				.WidthOverride(48)
				.HeightOverride(48)
				[
					SNew(SImage)
					.Image(this, &SUTMenuBase::GetFullvsWindowButtonImage)
				]
			]
		];

		RightMenuBar->AddSlot()
		.Padding(0.0f,0.0f,5.0f,0.0f)
		.AutoWidth()
		[
			SNew(SUTButton)
			.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
			.OnClicked(this, &SUTMenuBase::ExitClicked)
			[
				SNew(SBox)
				.WidthOverride(48)
				.HeightOverride(48)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.Icon.Exit"))
				]
			]
		];
	}


	return RightMenuBar.ToSharedRef();
}

EVisibility SUTMenuBase::GetSocialBangVisibility() const
{
	if (PlayerOwner.IsValid())
	{
		if (PlayerOwner->IsPlayerShowingSocialNotification())
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

TSharedRef<SWidget> SUTMenuBase::BuildOptionsSubMenu()
{
	
	TSharedPtr<SUTComboButton> DropDownButton = NULL;
	
	SNew(SHorizontalBox)
	+SHorizontalBox::Slot()
	.AutoWidth()
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.WidthOverride(48)
			.HeightOverride(48)
			[
				SAssignNew(DropDownButton, SUTComboButton)
				.HasDownArrow(false)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.ContentPadding(FMargin(0.0f, 0.0f))
				.ButtonContent()
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.Icon.Settings"))
				]
			]
		]
	];

	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_PlayerSettings", "Player Settings"), FOnClicked::CreateSP(this, &SUTMenuBase::OpenPlayerSettings));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_SocialSettings", "Social Settings"), FOnClicked::CreateSP(this, &SUTMenuBase::OpenSocialSettings));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_WeaponSettings", "Weapon Settings"), FOnClicked::CreateSP(this, &SUTMenuBase::OpenWeaponSettings));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_HUDSettings", "HUD Settings"), FOnClicked::CreateSP(this, &SUTMenuBase::OpenHUDSettings));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_SystemSettings", "System Settings"), FOnClicked::CreateSP(this, &SUTMenuBase::OpenSystemSettings));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ControlSettings", "Control Settings"), FOnClicked::CreateSP(this, &SUTMenuBase::OpenControlSettings));
	DropDownButton->AddSpacer();
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ClearCloud", "Reset Profile"), FOnClicked::CreateSP(this, &SUTMenuBase::ClearCloud), true);

	return DropDownButton.ToSharedRef();

}

TSharedRef<SWidget> SUTMenuBase::BuildAboutSubMenu()
{
	TSharedPtr<SUTComboButton> DropDownButton = NULL;

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
				SAssignNew(DropDownButton, SUTComboButton)
				.HasDownArrow(false)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.ButtonContent()
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.Icon.About"))
				]
			]
		]
	];


	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_TPSReport", "Third Party Software"), FOnClicked::CreateSP(this, &SUTMenuBase::OpenTPSReport));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_Credits", "Credits"), FOnClicked::CreateSP(this, &SUTMenuBase::OpenCredits));
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_UTSite", "UnrealTournament.com"), FOnClicked::CreateSP(this, &SUTMenuBase::OnMenuHTTPButton, FString(TEXT("http://www.unrealtournament.com/"))));

#if UE_BUILD_DEBUG
	DropDownButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_About_WR", "Widget Reflector"), FOnClicked::CreateSP(this, &SUTMenuBase::ShowWidgetReflector));
#endif
	DropDownButton->RebuildMenuContent();
	return DropDownButton.ToSharedRef();
}

void SUTMenuBase::BuildExitMenu(TSharedPtr<SUTComboButton> ExitButton)
{
}


FReply SUTMenuBase::OpenPlayerSettings()
{
	if (TickCountDown <= 0)
	{
		PlayerOwner->ShowContentLoadingMessage();
		bNeedsPlayerOptions = true;
		TickCountDown = 3;
	}

	return FReply::Handled();
}

FReply SUTMenuBase::OpenSocialSettings()
{
	PlayerOwner->OpenDialog(SNew(SUTSocialSettingsDialog).PlayerOwner(PlayerOwner));
	return FReply::Handled();
}


FReply SUTMenuBase::OpenWeaponSettings()
{
	if (TickCountDown <= 0)
	{
		PlayerOwner->ShowContentLoadingMessage();
		bNeedsWeaponOptions = true;
		TickCountDown = 3;
	}

	return FReply::Handled();
}

FReply SUTMenuBase::OpenSystemSettings()
{
	PlayerOwner->OpenDialog(SNew(SUTSystemSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop","System","System Settings")));
	return FReply::Handled();
}

FReply SUTMenuBase::OpenControlSettings()
{
	PlayerOwner->OpenDialog(SNew(SUTControlSettingsDialog).PlayerOwner(PlayerOwner).DialogTitle(NSLOCTEXT("SUWindowsDesktop","Controls","Control Settings")));
	return FReply::Handled();
}

FReply SUTMenuBase::OpenProfileItems()
{
	PlayerOwner->OpenDialog(SNew(SUTProfileItemsDialog).PlayerOwner(PlayerOwner));
	return FReply::Handled();
}

FReply SUTMenuBase::ClearCloud()
{
	if (PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->ClearProfileSettings();				
	}
	else
	{
		PlayerOwner->ShowMessage(NSLOCTEXT("SUTMainMenu","NotLoggedInTitle","Not Logged In"), NSLOCTEXT("SUTMainMenu","NoLoggedInMsg","You need to be logged in to clear your cloud settings!"), UTDIALOG_BUTTON_OK);
	}
	return FReply::Handled();
}


FReply SUTMenuBase::OpenTPSReport()
{
	PlayerOwner->OpenDialog(
							SNew(SUTMessageBoxDialog)
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

FReply SUTMenuBase::OpenCredits()
{
	TSharedPtr<class SUTCreditsPanel> CreditsPanel = PlayerOwner->GetCreditsPanel();
	if (CreditsPanel.IsValid())
	{
		ActivatePanel(CreditsPanel);
	}
	return FReply::Handled();
}

FReply SUTMenuBase::OnMenuHTTPButton(FString URL)
{
	FString Error;
	FPlatformProcess::LaunchURL(*URL, NULL, &Error);
	if (Error.Len() > 0)
	{
		PlayerOwner->OpenDialog(
								SNew(SUTMessageBoxDialog)
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
	TSharedPtr<class SUTStatsViewerPanel> StatsViewer = PlayerOwner->GetStatsViewer();
	if (StatsViewer.IsValid())
	{
		StatsViewer->SetQueryWindow(TEXT("alltime"));
		//StatsViewer->SetQueryWindow(TEXT("monthly"));
		//StatsViewer->SetQueryWindow(TEXT("weekly"));
		//StatsViewer->SetQueryWindow(TEXT("daily"));
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

		TSharedPtr<SUTComboButton> LogoutButton;
		TSharedPtr<SHorizontalBox> Box;

		SAssignNew(Box, SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.OnClicked(this, &SUTMenuBase::OnShowStatsViewer)
				.ToolTipText(NSLOCTEXT("ToolTips","TPMyStats","Show stats for this player, friends, and recent opponents."))
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
							.Image(SUTStyle::Get().GetBrush("UT.Icon.Stats"))
						]
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.OnClicked(this, &SUTMenuBase::OpenProfileItems)
				.ToolTipText(NSLOCTEXT("ToolTips", "TPMyItems", "Show collectable items you own."))
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
							.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.UpArrow"))
						]
					]
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
				.ToolTipText(NSLOCTEXT("ToolTips","TPMyPlayerCard","Show this player's player card."))
				.Text(FText::FromString(PlayerOwner->GetOnlinePlayerNickname()))
				.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
				.OnClicked(this, &SUTMenuBase::OnShowPlayerCard)
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
							.Image(SUTStyle::Get().GetBrush("UT.Icon.PlayerCard"))
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(48)
						.HeightOverride(48)
						[
							SAssignNew(LogoutButton, SUTComboButton)
							.HasDownArrow(false)
							.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
							.ContentPadding(FMargin(0.0f, 0.0f))
							.ButtonContent()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot().AutoHeight()
									[
										SNew(SBox).HeightOverride(12).WidthOverride(24)
										[
											SNew(SImage)
											.Image(SUTStyle::Get().GetBrush("UT.Icon.SortDown"))
										]
									]
								]
							]
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
					SNew(SUTButton)
					.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
					.OnClicked(this, &SUTMenuBase::ToggleFriendsAndChat)
#if PLATFORM_LINUX
					.ToolTipText(NSLOCTEXT("ToolTips", "TPFriendsNotSupported", "Friends list not supported yet on this platform."))
#else
					.ToolTipText(NSLOCTEXT("ToolTips","TPFriends","Show / Hide your friends list."))
#endif
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SBox)
								.WidthOverride(48)
								.HeightOverride(48)
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.Icon.Online"))
								]
							]
							+ SOverlay::Slot()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Top)
							[
								SNew(SImage)
								.Visibility(this, &SUTMenuBase::GetSocialBangVisibility)
								.Image(SUTStyle::Get().GetBrush("UT.Icon.SocialBang"))
							]
						]
					]

				]
			];

		if (LogoutButton.IsValid())
		{
			LogoutButton->AddSubMenuItem(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Logout_Logout", "Sign Out"), FOnClicked::CreateSP(this, &SUTMenuBase::Logout), true);
			// Add the additional account options here at some point.
				
		}

		return Box.ToSharedRef();
	
	}
	else
	{
		return 	SNew(SUTButton)
		.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
		.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_SignIn","Sign In"))
		.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
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
					.Image(SUTStyle::Get().GetBrush("UT.Icon.SignIn"))
				]

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
	PlayerOwner->GetAuth();
	return FReply::Handled();
}

FReply SUTMenuBase::OnShowServerBrowser()
{
	return OnShowServerBrowserPanel();
}


FReply SUTMenuBase::OnShowServerBrowserPanel()
{

	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline(TEXT(""), TEXT(""));
		return FReply::Handled();
	}
	

	TSharedPtr<class SUTServerBrowserPanel> Browser = PlayerOwner->GetServerBrowser();
	if (Browser.IsValid())
	{
		ActivatePanel(Browser);
	}

	return FReply::Handled();
}

FReply SUTMenuBase::ToggleFriendsAndChat()
{
#if PLATFORM_LINUX
	// Need launcher so this doesn't work on linux right now
	return FReply::Handled();
#endif

	if (bShowingFriends)
	{
		Desktop->RemoveSlot(6000);
		bShowingFriends = false;
	}
	else
	{
		TSharedPtr<SUTFriendsPopupWindow> Popup = PlayerOwner->GetFriendsPopup();
		Popup->SetOnCloseClicked(FOnClicked::CreateSP(this, &SUTMenuBase::ToggleFriendsAndChat));

		if (Popup.IsValid())
		{
			Desktop->AddSlot(6000)
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
		PlayerOwner->OpenDialog(SNew(SUTPlayerSettingsDialog).PlayerOwner(PlayerOwner));
		bNeedsPlayerOptions = false;
		PlayerOwner->HideContentLoadingMessage();
	}
	else if (bNeedsWeaponOptions)
	{
		PlayerOwner->OpenDialog(SNew(SUTWeaponConfigDialog).PlayerOwner(PlayerOwner));
		bNeedsWeaponOptions = false;
		PlayerOwner->HideContentLoadingMessage();
	}
}

void SUTMenuBase::ShowHomePanel()
{
	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
	else if (ActivePanel.IsValid())
	{
		DeactivatePanel(ActivePanel);
	}
}


FReply SUTMenuBase::OnShowHomePanel()
{
	ShowHomePanel();
	return FReply::Handled();
}

FReply SUTMenuBase::ShowWidgetReflector()
{
	ConsoleCommand(TEXT("WidgetReflector"));
	return FReply::Handled();
}

TSharedRef<SWidget> SUTMenuBase::BuildBackground()
{
	return SNullWidget::NullWidget;
}

FReply SUTMenuBase::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent )
{
	if (InKeyboardEvent.GetKey() == EKeys::Escape)
	{
		if (bShowingFriends)
		{
			ToggleFriendsAndChat();
		}
	}

	return SUWindowsDesktop::OnKeyUp(MyGeometry, InKeyboardEvent);
}

FReply SUTMenuBase::OpenHUDSettings()
{
	PlayerOwner->ShowHUDSettings();
	return FReply::Handled();
}

FReply SUTMenuBase::OnShowPlayerCard()
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
	{
		AUTPlayerState* PlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (PlayerState)
		{
			PlayerOwner->ShowPlayerInfo(PlayerState);
		}
	}
	return FReply::Handled();
}

const FSlateBrush* SUTMenuBase::GetFullvsWindowButtonImage() const
{
	if (PlayerOwner->ViewportClient->IsFullScreenViewport())
	{
		return SUTStyle::Get().GetBrush("UT.Icon.Windowed");
	}
	else
	{
		return SUTStyle::Get().GetBrush("UT.Icon.Fullscreen");
	}
}

FReply SUTMenuBase::ToggleFullscreenClicked()
{
	if (PlayerOwner->ViewportClient->IsFullScreenViewport())
	{
		PlayerOwner->ConsoleCommand("fullscreen 0");
	}
	else
	{
		PlayerOwner->ConsoleCommand("fullscreen 1");
	}
	return FReply::Handled();
}

FReply SUTMenuBase::MinimizeClicked()
{
	FPlatformMisc::RequestMinimize();
	return FReply::Handled();
}

EVisibility SUTMenuBase::GetBackVis() const
{
	if (ActivePanel.IsValid() && ActivePanel->ShouldShowBackButton())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

FReply SUTMenuBase::ExitClicked()
{
	QuitConfirmation();
	return FReply::Handled();
}

void SUTMenuBase::QuitConfirmation()
{
	PlayerOwner->ShowMessage(NSLOCTEXT("SUTMenuBase", "QuitGameSureTitle", "Quit Game"), NSLOCTEXT("SUTMenuBase", "QuitGameSureMessage", "You are about to quit the game.  Are you sure?"), UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateSP(this, &SUTMenuBase::QuitConfirmationResult));
}

void SUTMenuBase::QuitConfirmationResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		PlayerOwner->ConsoleCommand(TEXT("quit"));
	}
}

FReply SUTMenuBase::Logout()
{
	PlayerOwner->ShowMessage(NSLOCTEXT("SUTMenuBase", "SignOuttConfirmationTitle", "Sign Out?"), NSLOCTEXT("SUTMenuBase", "SignOuttConfirmationMessage", "You are about to sign out of this account.  Doing so will return you to the main menu.  Are you sure?"), UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateSP(this, &SUTMenuBase::SignOutConfirmationResult));
	return FReply::Handled();
}

void SUTMenuBase::SignOutConfirmationResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		PlayerOwner->Logout();
	}
}

TSharedPtr<SUTPanelBase> SUTMenuBase::GetActivePanel()
{
	return ActivePanel;
}

#endif