// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTMainMenu.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "../Base/SUTDialogBase.h"
#include "../Dialogs/SUTSystemSettingsDialog.h"
#include "../Dialogs/SUTPlayerSettingsDialog.h"
#include "../Dialogs/SUTHUDSettingsDialog.h"
#include "../Dialogs/SUTWeaponConfigDialog.h"
#include "../Dialogs/SUTControlSettingsDialog.h"
#include "../Dialogs/SUTInputBoxDialog.h"
#include "../Dialogs/SUTMessageBoxDialog.h"
#include "../Widgets/SUTScaleBox.h"
#include "UTGameEngine.h"
#include "../Menus/SUTInGameMenu.h"
#include "../Panels/SUTInGameHomePanel.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "../Dialogs/SUTQuickPickDialog.h"
#include "UTLobbyGameState.h"

#if !UE_SERVER

/****************************** [ Build Sub Menus ] *****************************************/

void SUTInGameMenu::BuildLeftMenuBar()
{
	if (LeftMenuBar.IsValid())
	{
		AUTGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
		AUTPlayerState* PS = PlayerOwner->PlayerController ? Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState) : NULL;
		bool bIsSpectator = PS && PS->bOnlySpectator;
		if (GS && GS->bTeamGame && !bIsSpectator)
		{
			LeftMenuBar->AddSlot()
			.Padding(5.0f,0.0f,0.0f,0.0f)
			.AutoWidth()
			[
				SNew(SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.OnClicked(this, &SUTInGameMenu::OnTeamChangeClick)
				.Visibility(this, &SUTInGameMenu::GetChangeTeamVisibility)
				.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUTMenuBase","MenuBar_ChangeTeam","CHANGE TEAM"))
						.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
					]
				]
			];
		}


		if (GS && GS->GetMatchState() == MatchState::WaitingToStart)
		{
			if (GS->GetNetMode() == NM_Standalone)
			{
				LeftMenuBar->AddSlot()
					.Padding(5.0f, 0.0f, 0.0f, 0.0f)
					.AutoWidth()
					[
						SNew(SUTButton)
						.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
						.OnClicked(this, &SUTInGameMenu::OnReadyChangeClick)
						.ContentPadding(FMargin(25.0, 0.0, 25.0, 5.0))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("SUTMenuBase", "MenuBar_StartMatch", "START MATCH"))
								.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
							]
						]
					];
			}
			else if (!bIsSpectator)
			{
				LeftMenuBar->AddSlot()
					.Padding(5.0f, 0.0f, 0.0f, 0.0f)
					.AutoWidth()
					[
						SNew(SUTButton)
						.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
						.OnClicked(this, &SUTInGameMenu::OnReadyChangeClick)
						.ContentPadding(FMargin(25.0, 0.0, 25.0, 5.0))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("SUTMenuBase", "MenuBar_ChangeReady", "CHANGE READY"))
								.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
							]
						]
					];
			}
		}
		else if (GS && GS->GetMatchState() == MatchState::MapVoteHappening)
		{
			LeftMenuBar->AddSlot()
			.Padding(5.0f,0.0f,0.0f,0.0f)
			.AutoWidth()
			[
				SNew(SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.OnClicked(this, &SUTInGameMenu::OnMapVoteClick)
				.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SUTInGameMenu::GetMapVoteTitle)
						.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
					]
				]
			];

			PlayerOwner->OpenMapVote(NULL);
		}

		LeftMenuBar->AddSlot()
		.Padding(5.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SUTButton)
			.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
			.OnClicked(this, &SUTInGameMenu::ShowSummary)
			.Visibility(this, &SUTInGameMenu::GetMatchSummaryButtonVisibility)
			.ContentPadding(FMargin(25.0,0.0,25.0,5.0))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUTMenuBase","MenuBar_MatchSummary","MATCH SUMMARY"))
					.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
				]
			]
		];


	}
	
	FSlateApplication::Get().PlaySound(SUTStyle::PauseSound,0);

}

void SUTInGameMenu::BuildExitMenu(TSharedPtr<SUTComboButton> ExitButton)
{
	ExitButton->AddSubMenuItem(NSLOCTEXT("SUTMenuBase", "MenuBar_Exit_ReturnToGame", "Close Menu"), FOnClicked::CreateSP(this, &SUTInGameMenu::OnCloseMenu));
	ExitButton->AddSpacer();
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if ( GameState && GameState->HubGuid.IsValid() )
	{
		ExitButton->AddSubMenuItem(NSLOCTEXT("SUTInGameMenu", "MenuBar_ReturnToLobby", "Return to Hub"), FOnClicked::CreateSP(this, &SUTInGameMenu::OnReturnToLobby));
	}

	ExitButton->AddSubMenuItem(NSLOCTEXT("SUTInGameMenu","MenuBar_ReturnToMainMenu","Return to Main Menu"), FOnClicked::CreateSP(this, &SUTInGameMenu::OnReturnToMainMenu));
}

FReply SUTInGameMenu::OnCloseMenu()
{
	CloseMenus();
	return FReply::Handled();
}

FReply SUTInGameMenu::OnReturnToLobby()
{
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if ( GameState && GameState->HubGuid.IsValid() )
	{
		CloseMenus();
		AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
		if (PC)
		{
			WriteQuitMidGameAnalytics();
			PlayerOwner->CloseMapVote();
			PC->ConnectToServerViaGUID(GameState->HubGuid.ToString(),-1, false);
		}
	}

	return FReply::Handled();
}

void SUTInGameMenu::WriteQuitMidGameAnalytics()
{
	if (FUTAnalytics::IsAvailable() && PlayerOwner->GetWorld()->GetNetMode() != NM_Standalone)
	{
		AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
		if (GameState->HasMatchStarted() && !GameState->HasMatchEnded())
		{
			AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
			if (PC)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(PC->PlayerState);
				if (PS)
				{
					extern float ENGINE_API GAverageFPS;
					TArray<FAnalyticsEventAttribute> ParamArray;
					ParamArray.Add(FAnalyticsEventAttribute(TEXT("FPS"), GAverageFPS));
					ParamArray.Add(FAnalyticsEventAttribute(TEXT("Kills"), PS->Kills));
					ParamArray.Add(FAnalyticsEventAttribute(TEXT("Deaths"), PS->Deaths));
					FUTAnalytics::GetProvider().RecordEvent(TEXT("QuitMidGame"), ParamArray);
				}
			}
		}
	}
}

FReply SUTInGameMenu::OnReturnToMainMenu()
{
	WriteQuitMidGameAnalytics();
	PlayerOwner->CloseMapVote();
	CloseMenus();
	PlayerOwner->ReturnToMainMenu();
	return FReply::Handled();
}

FReply SUTInGameMenu::OnTeamChangeClick()
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC)
	{
		PC->ServerSwitchTeam();
	}
	return FReply::Handled();
}

FReply SUTInGameMenu::OnReadyChangeClick()
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC)
	{
//		PC->PlayMenuSelectSound();
		PC->ServerRestartPlayer();
		PlayerOwner->HideMenu();
	}
	return FReply::Handled();
}

FReply SUTInGameMenu::OnSpectateClick()
{
	ConsoleCommand(TEXT("ChangeTeam 255"));
	return FReply::Handled();
}


void SUTInGameMenu::SetInitialPanel()
{
	SAssignNew(HomePanel, SUTInGameHomePanel, PlayerOwner);

	if (HomePanel.IsValid())
	{
		ActivatePanel(HomePanel);
	}
}

FReply SUTInGameMenu::OpenHUDSettings()
{
	CloseMenus();
	PlayerOwner->ShowHUDSettings();
	return FReply::Handled();
}

FReply SUTInGameMenu::OnMapVoteClick()
{
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if ( GameState )
	{
		PlayerOwner->OpenMapVote(GameState);
	}

	return FReply::Handled();
}

FText SUTInGameMenu::GetMapVoteTitle() const
{
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if ( GameState )
	{
		return FText::Format(NSLOCTEXT("SUTInGameMenu","MapVoteFormat","MAP VOTE ({0})"), FText::AsNumber(GameState->VoteTimer));
	}

	return NSLOCTEXT("SUTMenuBase","MenuBar_MapVote","MAP VOTE");
}


void SUTInGameMenu::ShowExitDestinationMenu()
{
	TSharedPtr<SUTQuickPickDialog> QP;

	SAssignNew(QP, SUTQuickPickDialog)
		.PlayerOwner(PlayerOwner)
		.OnPickResult(this, &SUTInGameMenu::OnDestinationResult)
		.DialogTitle(NSLOCTEXT("SUTMenuBase","Test","Choose your Destination..."));

	QP->AddOption(
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExitMenu", "ExitGameTitle", "EXIT GAME"))
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExitMenu", "ExitGameMessage", "Exit the game and return to the opperating system."))
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
			.AutoWrapText(true)
		]
	);		


	QP->AddOption(
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExitMenu", "MainMenuTitle", "MAIN MENU"))
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ExitMenu", "MainMenuMessage", "Leave the current online game and return to the main menu."))
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
			.AutoWrapText(true)
		]
	, true
	);		

	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState && GameState->bIsInstanceServer)
	{
		QP->AddOption(
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("ExitMenu", "HubTitle", "RETURN TO HUB"))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("ExitMenu", "HubMessage", "Exit the current online game and return to the main hub."))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.AutoWrapText(true)
			]
		);		
	}

	PlayerOwner->OpenDialog(QP.ToSharedRef(), 200);

}

void SUTInGameMenu::QuitConfirmation()
{
	ShowExitDestinationMenu();
}

void SUTInGameMenu::OnDestinationResult(int32 PickedIndex)
{
	switch(PickedIndex)
	{
		case 0 : PlayerOwner->ConsoleCommand(TEXT("quit")); break;
		case 1 : OnReturnToMainMenu(); break;
		case 2 : OnReturnToLobby(); break;
	}
}

void SUTInGameMenu::ShowHomePanel()
{
	// If we are on the home panel, look to see if it's time to go backwards.
	if (ActivePanel == HomePanel)
	{
		AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
		bool bInstanceServer = (GameState && GameState->bIsInstanceServer);

		FText Msg;

		if (bInstanceServer)
		{
			Msg = NSLOCTEXT("SUTInGameMenu", "SUTInGameMenuBackLobby", "Are you sure you want to leave the game and return to the hub?");
		}
		else
		{
			if (PlayerOwner->GetWorld()->GetGameState<AUTLobbyGameState>())
			{
				Msg = NSLOCTEXT("SUTInGameMenu", "SUTInGameMenuBack", "Are you sure you want to leave the hub and return to the main menu?");
			}
			else
			{
				Msg = NSLOCTEXT("SUTInGameMenu", "SUTInGameMenuBackDefault", "Are you sure you want to leave the game and return to the main menu?");
			}
		}

		PlayerOwner->OpenDialog(
								SNew(SUTMessageBoxDialog)
								.PlayerOwner(PlayerOwner)
								.DialogTitle(NSLOCTEXT("SUTInGameMenu", "SUTInGameMenuBackTitle", "Confirmation"))
								.OnDialogResult(this, &SUTInGameMenu::BackResult)
								.MessageText( Msg )
								.ButtonMask(UTDIALOG_BUTTON_YES | UTDIALOG_BUTTON_NO)
								);
	}
	else
	{
		SUTMenuBase::ShowHomePanel();
	}
}

void SUTInGameMenu::BackResult(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonPressed)
{
	if (ButtonPressed == UTDIALOG_BUTTON_YES)
	{
		AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
		if (GameState && GameState->bIsInstanceServer)
		{
			OnReturnToLobby();
		}
		else
		{
			OnReturnToMainMenu();
		}
	}
}

EVisibility SUTInGameMenu::GetChangeTeamVisibility() const
{
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState && GameState->GetMatchState() != MatchState::WaitingPostMatch && GameState->GetMatchState() != MatchState::PlayerIntro)
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

EVisibility SUTInGameMenu::GetMatchSummaryVisibility() const
{
	return StaticCastSharedPtr<SUTInGameHomePanel>(HomePanel)->GetSummaryVisibility();
}

EVisibility SUTInGameMenu::GetMatchSummaryButtonVisibility() const
{
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState && GameState->GetMatchState() == MatchState::WaitingPostMatch)
	{
		if ( GetMatchSummaryVisibility() == EVisibility::Hidden)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}


FReply SUTInGameMenu::ShowSummary()
{
	StaticCastSharedPtr<SUTInGameHomePanel>(HomePanel)->ShowMatchSummary(false);
	return FReply::Handled();
}

void SUTInGameMenu::OnMenuOpened(const FString& Parameters)
{
	SUTMenuBase::OnMenuOpened(Parameters);
	if (Parameters.Equals(TEXT("forcesummary"),ESearchCase::IgnoreCase))
	{
		StaticCastSharedPtr<SUTInGameHomePanel>(HomePanel)->ShowMatchSummary(true);
	}

	if (Parameters.Equals(TEXT("say"),ESearchCase::IgnoreCase) || Parameters.Equals(TEXT("teamsay"),ESearchCase::IgnoreCase))
	{
		if (Parameters.Equals(TEXT("teamsay"),ESearchCase::IgnoreCase))
		{
			StaticCastSharedPtr<SUTInGameHomePanel>(HomePanel)->SetChatDestination(ChatDestinations::Team);
		}
		StaticCastSharedPtr<SUTInGameHomePanel>(HomePanel)->bCloseOnSubmit = true;
		StaticCastSharedPtr<SUTInGameHomePanel>(HomePanel)->FocusChat();
	}
}

bool SUTInGameMenu::SkipWorldRender()
{
	return GetMatchSummaryVisibility() == EVisibility::Visible;
}



#endif