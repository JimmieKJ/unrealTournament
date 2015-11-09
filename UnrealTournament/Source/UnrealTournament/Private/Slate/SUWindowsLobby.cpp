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
	SAssignNew(InfoPanel, SULobbyInfoPanel, PlayerOwner);
	HomePanel = InfoPanel;

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


void SUWindowsLobby::BuildExitMenu(TSharedPtr<SUTComboButton> ExitButton)
{
	ExitButton->AddSubMenuItem(NSLOCTEXT("SUTInGameMenu","MenuBar_ReturnToMainMenu","Return to Main Menu"), FOnClicked::CreateSP(this, &SUTInGameMenu::OnReturnToMainMenu));
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
	return SUTMenuBase::BuildOptionsSubMenu();
}

FReply SUWindowsLobby::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	if (InfoPanel.IsValid()) InfoPanel->FocusChat(InCharacterEvent);
	return FReply::Unhandled();
}


#endif