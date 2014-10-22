// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "Slate.h"
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

void SUWindowsLobby::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SUWindowsMidGame::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
	if (LobbyGameState)
	{
		FText LobbyStatus = FText::Format(NSLOCTEXT("LOBBY","MatchStatus","# Available Matches: {0}"), FText::AsNumber(LobbyGameState->AvailableMatches.Num()));
		LeftStatusText->SetText(LobbyStatus);
	}
}

TSharedRef<SWidget> SUWindowsLobby::BuildMenuBar()
{
	SAssignNew(MenuBar, SHorizontalBox);
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
					.Text(NSLOCTEXT("Gerneric","NewMatch","NEW MATCH"))
					.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button.TextStyle"))
				]
			];


	}

	return MenuBar.ToSharedRef();
}

void SUWindowsLobby::BuildInfoPanel()
{
	if (!InfoPanel.IsValid())
	{
		SAssignNew(InfoPanel, SULobbyInfoPanel).PlayerOwner(PlayerOwner);
	}
}

FReply SUWindowsLobby::MatchButtonClicked()
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
		AUTLobbyGameState* GameState = GWorld->GetGameState<AUTLobbyGameState>();
		if (PlayerState && GameState)
		{
			GameState->AddMatch(PlayerState);		
		}
	}
	return FReply::Handled();
}

#endif