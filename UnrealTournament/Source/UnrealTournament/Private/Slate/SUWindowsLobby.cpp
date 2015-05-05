// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsLobby.h"
#include "SUWindowsStyle.h"
#include "SUWSystemSettingsDialog.h"
#include "SUWPlayerSettingsDialog.h"
#include "SUWControlSettingsDialog.h"
#include "SUWInputBox.h"
#include "SUWMessageBox.h"
#include "SUWScaleBox.h"
#include "UTGameEngine.h"
#include "Panels/SUWServerBrowser.h"
#include "UTLobbyGameState.h"
#include "Panels/SUMidGameInfoPanel.h"


#if !UE_SERVER

void SUWindowsLobby::SetInitialPanel()
{
	SAssignNew(HomePanel, SULobbyInfoPanel, PlayerOwner);

	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
}


FText SUWindowsLobby::GetDisconnectButtonText() const
{
	return NSLOCTEXT("SUWindowsLobby","MenuBar_LeaveHUB","LEAVE HUB");
}



FReply SUWindowsLobby::MatchButtonClicked()
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (PlayerState)
		{
			PlayerState->MatchButtonPressed();
		}
	}
	return FReply::Handled();
}

FText SUWindowsLobby::GetMatchButtonText() const
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->PlayerState)
	{
		AUTLobbyPlayerState* LPS = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (LPS)
		{
			if (LPS->CurrentMatch)
			{
				return NSLOCTEXT("Gerneric","LeaveMatch","LEAVE MATCH");
			}
			return NSLOCTEXT("Gerneric","NewMatch","NEW MATCH");
	
		}
	}
	return FText::GetEmpty();
}

FString SUWindowsLobby::GetMatchCount() const
{
	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		int32 NoActiveMatches = 0;
		for (int32 i=0;i<LobbyGameState->AvailableMatches.Num();i++)
		{
			if (LobbyGameState->AvailableMatches[i])
			{
				if (LobbyGameState->AvailableMatches[i]->CurrentState == ELobbyMatchState::WaitingForPlayers || LobbyGameState->AvailableMatches[i]->CurrentState == ELobbyMatchState::InProgress || LobbyGameState->AvailableMatches[i]->CurrentState == ELobbyMatchState::Launching )
				{
					NoActiveMatches++;
				}
			}
		}
		return FString::Printf(TEXT("%s - There are %i matches available."), *LobbyGameState->ServerName,NoActiveMatches);
	}

	return TEXT("Loading Lobby...");
}


void SUWindowsLobby::BuildExitMenu(TSharedPtr<SComboButton> ExitButton, TSharedPtr<SVerticalBox> MenuSpace)
{
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

TSharedRef<SWidget> SUWindowsLobby::BuildBackground()
{
	return SNew(SOverlay)
	+SOverlay::Slot()
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
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
					.Image(SUWindowsStyle::Get().GetBrush("UT.Backgrounds.BK01"))
				]
			]
		]
	]
	+SOverlay::Slot()
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
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
					.Image(SUWindowsStyle::Get().GetBrush("UT.Backgrounds.Overlay"))
				]
			]
		]
	];
}

TSharedRef<SWidget> SUWindowsLobby::BuildOptionsSubMenu()
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
				.Text(NSLOCTEXT("SUWindowsDesktop", "MenuBar_Options_ClearCloud", "Clear Game Settings").ToString())
				.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
				.OnClicked(this, &SUTMenuBase::ClearCloud, DropDownButton)
			]
		]
	);

	MenuButtons.Add(DropDownButton);
	return DropDownButton.ToSharedRef();

}



#endif