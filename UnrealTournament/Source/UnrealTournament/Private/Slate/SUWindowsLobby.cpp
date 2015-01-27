// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsMidGame.h"
#include "SUWindowsLobby.h"
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


#if !UE_SERVER

void SUWindowsLobby::BuildTopBar()
{
	SUInGameMenu::BuildTopBar();

	// Add the status text
	if (TopOverlay.IsValid())
	{
		TopOverlay->AddSlot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.Padding(0,3,0,0)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.Padding(5,0,25,0)
				[
					SNew(STextBlock)
					.Text(this, &SUWindowsLobby::GetMatchCount)
					.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Status.TextStyle"))
				]
			]
		];
	}
}

TSharedRef<SWidget> SUWindowsLobby::BuildMenuBar()
{
	if (MenuBar.IsValid())
	{

		BuildInfoSubMenu();
		BuildExitMatchSubMenu();
		BuildServerBrowserSubMenu();
		BuildOptionsSubMenu();

		MenuBar->AddSlot()
			.AutoWidth()
			.Padding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			[
				SAssignNew(MatchButton,SButton)
				.VAlign(VAlign_Center)
				.OnClicked(this, &SUWindowsLobby::MatchButtonClicked)
				.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
				[
					SNew(STextBlock)
					.Text(this, &SUWindowsLobby::GetMatchButtonText) //NSLOCTEXT("Gerneric","NewMatch","NEW MATCH"))
					.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
				]
			];


	}

	return MenuBar.ToSharedRef();
}

void SUWindowsLobby::BuildDesktop()
{
	if (!DesktopPanel.IsValid())
	{
		SAssignNew(DesktopPanel, SULobbyInfoPanel).PlayerOwner(PlayerOwner);
	}

	if (DesktopPanel.IsValid())
	{
		ActivatePanel(DesktopPanel);
	}
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


#endif