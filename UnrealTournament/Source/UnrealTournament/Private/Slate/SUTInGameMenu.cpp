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
#include "Panels/SUInGameHomePanel.h"

#if !UE_SERVER

/****************************** [ Build Sub Menus ] *****************************************/

void SUTInGameMenu::BuildLeftMenuBar()
{
	if (LeftMenuBar.IsValid())
	{
		LeftMenuBar->AddSlot()
		.Padding(5.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Left")
			.OnClicked(this, &SUTInGameMenu::OnDisconnect)
			.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SUTInGameMenu::GetDisconnectButtonText)
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
				]
			]
		];

		AUTGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
		if (GS && GS->bTeamGame)
		{
			LeftMenuBar->AddSlot()
			.Padding(5.0f,0.0f,0.0f,0.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Left")
				.OnClicked(this, &SUTInGameMenu::OnTeamChangeClick)
				.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_ChangeTeam","CHANGE TEAM").ToString())
						.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
					]
				]
			];
		}
/*
		LeftMenuBar->AddSlot()
		.Padding(5.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.Left")
			.OnClicked(this, &SUTInGameMenu::OnSpectateClick)
			.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWindowsDesktop","MenuBar_Spectate","SPECTATE").ToString())
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
				]
			]
		];
*/
	}
}


FText SUTInGameMenu::GetDisconnectButtonText() const
{
	if (PlayerOwner->LastLobbyServerGUID != TEXT(""))
	{
		return NSLOCTEXT("SUTInGameMenu","MenuBar_ReturnToLobby","RETURN TO LOBBY");
	}

	return NSLOCTEXT("SUTInGameMenu","MenuBar_QuitMatch","QUIT MATCH");
}

FReply SUTInGameMenu::OnDisconnect()
{
	CloseMenus();
	if (PlayerOwner->LastLobbyServerGUID != TEXT(""))
	{
		// Connect to that GUID
		AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
		if (PC)
		{
			PC->ConnectToServerViaGUID(PlayerOwner->LastLobbyServerGUID, false);
			PlayerOwner->LastLobbyServerGUID = TEXT("");
		}
	}

	else
	{
		PlayerOwner->ReturnToMainMenu();
	}

	return FReply::Handled();
}
FReply SUTInGameMenu::OnTeamChangeClick()
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC)
	{
		PC->SwitchTeam();
	}
	return FReply::Handled();
}

FReply SUTInGameMenu::OnSpectateClick()
{
	ConsoleCommand(TEXT("ChangeTeam 255"));
	return FReply::Handled();
}

FReply SUTInGameMenu::OnCloseClicked()
{
	PlayerOwner->HideMenu();
	return FReply::Handled();
}

void SUTInGameMenu::SetInitialPanel()
{
	SAssignNew(HomePanel, SUInGameHomePanel)
		.PlayerOwner(PlayerOwner);

	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
}



#endif