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
	SUTMenuBase::CreateDesktop();
}

void SUWindowsMainMenu::SetInitialPanel()
{
	SAssignNew(HomePanel, SUHomePanel)
		.PlayerOwner(PlayerOwner);

	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
}

/****************************** [ Build Sub Menus ] *****************************************/

void SUWindowsMainMenu::BuildLeftMenuBar()
{
	if (LeftMenuBar.IsValid())
	{
		LeftMenuBar->AddSlot()
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		.AutoWidth()
		[
			AddPlayNow()
		];

		LeftMenuBar->AddSlot()
		.Padding(5.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			BuildTutorialSubMenu()
		];

	}
}

TSharedRef<SWidget> SUWindowsMainMenu::BuildTutorialSubMenu()
{

	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Left")
		.ContentPadding(FMargin(25.0,0.0,25.0,0.0))
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_TUTORIAL", "LEARN").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
			]
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
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Tutorial_LeanHoToPlay", "Basic Training").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OnBootCampClick, DropDownButton)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(16)
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button.Spacer")
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Tutorial_Community", "Training Videos").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OnCommunityClick, DropDownButton)
		]
	);

	MenuButtons.Add(DropDownButton);
	return DropDownButton.ToSharedRef();

}


TSharedRef<SWidget> SUWindowsMainMenu::AddPlayNow()
{
	TSharedPtr<SComboButton> DropDownButton = NULL;

	SAssignNew(DropDownButton, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Left")
		.ContentPadding(FMargin(25.0,0.0,25.0,0.0))
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch", "PLAY").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
			]
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
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_PlayDM", "QuickPlay Deathmatch").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OnPlayQuickMatch, DropDownButton, QuickMatchTypes::Deathmatch)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_PlayCTF", "QuickPlay Capture the Flag").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OnPlayQuickMatch, DropDownButton, QuickMatchTypes::CaptureTheFlag)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(16)
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button.Spacer")
			]
		]
		 
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_CreateGame", "Create a Game").ToString())
			.OnClicked(this, &SUWindowsMainMenu::OnShowGamePanel, DropDownButton)
		]


		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(16)
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button.Spacer")
			]
		]


		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_FindGame", "Find a Game...").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUTMenuBase::OnShowServerBrowser, DropDownButton)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
			.ContentPadding(FMargin(10.0f, 5.0f))
			.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_QuickMatch_IPConnect", "Connect via IP").ToString())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
			.OnClicked(this, &SUWindowsMainMenu::OnConnectIP, DropDownButton)
		]

	);

	MenuButtons.Add(DropDownButton);
	return DropDownButton.ToSharedRef();
}

FReply SUWindowsMainMenu::OnCloseClicked()
{
	PlayerOwner->HideMenu();
	ConsoleCommand(TEXT("quit"));
	return FReply::Handled();
}



FReply SUWindowsMainMenu::OnShowGamePanel(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) 
	{
		MenuButton->SetIsOpen(false);
	}

	if (TickCountDown <= 0)
	{
		if (GamePanel.IsValid())
		{
			ActivatePanel(GamePanel);
		}
		else
		{
			PlayerOwner->ShowContentLoadingMessage();
			bNeedToShowGamePanel = true;
			TickCountDown = 3;
		}
	}

	return FReply::Handled();
}

void SUWindowsMainMenu::OpenDelayedMenu()
{
	SUTMenuBase::OpenDelayedMenu();
	if (bNeedToShowGamePanel)
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
	
}

FReply SUWindowsMainMenu::OnTutorialClick()
{
	ConsoleCommand(TEXT("Open " + PlayerOwner->TutorialLaunchParams));
	return FReply::Handled();
}


FReply SUWindowsMainMenu::OnPlayQuickMatch(TSharedPtr<SComboButton> MenuButton, FName QuickMatchType)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);

	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline(TEXT(""),TEXT(""));
		return FReply::Handled();
	}

	UE_LOG(UT,Log,TEXT("QuickMatch: %s"),*QuickMatchType.ToString());
	PlayerOwner->StartQuickMatch(QuickMatchType);
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnBootCampClick(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	ConsoleCommand(TEXT("open TUT-BasicTraining?timelimit=0"));
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnCommunityClick(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	if ( !WebPanel.IsValid() )
	{
		// Create the Web panel
		SAssignNew(WebPanel, SUTWebBrowserPanel)
			.PlayerOwner(PlayerOwner);
	}

	if (WebPanel.IsValid())
	{
		if (ActivePanel.IsValid() && ActivePanel != WebPanel)
		{
			ActivatePanel(WebPanel);
		}
		WebPanel->Browse(CommunityVideoURL);
	}
	return FReply::Handled();
}

FReply SUWindowsMainMenu::OnConnectIP(TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	PlayerOwner->OpenDialog(
							SNew(SUWInputBox)
							.DefaultInput(PlayerOwner->LastConnectToIP)
							.DialogSize(FVector2D(600, 200))
							.OnDialogResult(this, &SUWindowsMainMenu::ConnectIPDialogResult)
							.PlayerOwner(PlayerOwner)
							.DialogTitle(NSLOCTEXT("SUWindowsDesktop", "ConnectToIP", "Connect to IP"))
							.MessageText(NSLOCTEXT("SUWindowsDesktop", "ConnecToIPDesc", "Enter address to connect to:"))
							.ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
							);
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
				PlayerOwner->HideMenu();
			}
		}
	}
}

bool SUWindowsMainMenu::ShouldShowBrowserIcon()
{
	return (PlayerOwner.IsValid() && PlayerOwner->bShowBrowserIconOnMainMenu);
}


#endif