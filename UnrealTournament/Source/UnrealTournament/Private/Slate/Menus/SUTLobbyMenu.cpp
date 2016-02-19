// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTLobbyMenu.h"
#include "SUWindowsStyle.h"
#include "../Dialogs/SUTSystemSettingsDialog.h"
#include "../Dialogs/SUTPlayerSettingsDialog.h"
#include "../Dialogs/SUTControlSettingsDialog.h"
#include "../Dialogs/SUTInputBoxDialog.h"
#include "../Dialogs/SUTMessageBoxDialog.h"
#include "../Widgets/SUTScaleBox.h"
#include "UTGameEngine.h"
#include "../Panels/SUTServerBrowserPanel.h"
#include "UTLobbyGameState.h"
#include "../Panels/SUTMidGameInfoPanel.h"


#if !UE_SERVER

void SUTLobbyMenu::SetInitialPanel()
{
	SAssignNew(InfoPanel, SUTLobbyInfoPanel, PlayerOwner);
	HomePanel = InfoPanel;

	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
}


FText SUTLobbyMenu::GetDisconnectButtonText() const
{
	return NSLOCTEXT("SUTLobbyMenu","MenuBar_LeaveHUB","LEAVE HUB");
}



FReply SUTLobbyMenu::MatchButtonClicked()
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

FText SUTLobbyMenu::GetMatchButtonText() const
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

FString SUTLobbyMenu::GetMatchCount() const
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


void SUTLobbyMenu::BuildExitMenu(TSharedPtr<SUTComboButton> ExitButton)
{
	ExitButton->AddSubMenuItem(NSLOCTEXT("SUTInGameMenu","MenuBar_ReturnToMainMenu","Return to Main Menu"), FOnClicked::CreateSP(this, &SUTInGameMenu::OnReturnToMainMenu));
}

TSharedRef<SWidget> SUTLobbyMenu::BuildBackground()
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
				SNew(SUTScaleBox)
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
				SNew(SUTScaleBox)
				.bMaintainAspectRatio(false)
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UT.Backgrounds.Overlay"))
				]
			]
		]
	];
}

TSharedRef<SWidget> SUTLobbyMenu::BuildOptionsSubMenu()
{
	return SUTMenuBase::BuildOptionsSubMenu();
}

FReply SUTLobbyMenu::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	return FReply::Unhandled();
}

FReply SUTLobbyMenu::OpenHUDSettings()
{
	PlayerOwner->ShowHUDSettings();
	return FReply::Handled();
}




#endif