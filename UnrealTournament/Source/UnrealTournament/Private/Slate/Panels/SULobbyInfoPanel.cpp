
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMidGameInfoPanel.h"
#include "SULobbyMatchSetupPanel.h"
#include "SULobbyInfoPanel.h"
#include "UTLobbyPC.h"



#if !UE_SERVER

AUTLobbyPlayerState* SULobbyInfoPanel::GetOwnerPlayerState()
{
	AUTLobbyPC* PC = Cast<AUTLobbyPC>(PlayerOwner->PlayerController);
	if (PC) 
	{
		return PC->UTLobbyPlayerState;
	}
	return NULL;
}

void SULobbyInfoPanel::ConstructPanel(FVector2D ViewportSize)
{
	SUChatPanel::ConstructPanel(ViewportSize);
	Tag = FName(TEXT("LobbyInfoPanel"));
	bShowingMatchDock = false;
}

void SULobbyInfoPanel::BuildNonChatPanel()
{
	// Clear out any existing panels
	NonChatPanel->ClearChildren();

	AUTLobbyPlayerState* PlayerState = GetOwnerPlayerState();

	if (NonChatPanel.IsValid() && PlayerState)
	{
		if (bShowingMatchDock)
		{
			NonChatPanel->AddSlot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()		
						.HAlign(HAlign_Fill)
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.DarkBackground"))
							]
							+SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()		
								.AutoHeight()
								[
									SNew(SScrollBox)
									.Orientation(Orient_Horizontal)
									+SScrollBox::Slot()
									[
										SAssignNew(MatchPanelDock, SHorizontalBox)
										+SHorizontalBox::Slot()
										.Padding(5.0f,0.0f,5.0f,0.0f)
										[
											SNew(SCanvas)
										]
									]
								]
							]
						]			
				];
		}
		else

		// We need to show the match setup panel.  Hosts will have an interactive panel, non-hosts will just get an info dump

		{
			NonChatPanel->AddSlot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()		
					.AutoHeight()
					.VAlign(VAlign_Top)
					.HAlign(HAlign_Fill)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("Blue.UWindows.MidGameMenu.Bar.Top"))
						]
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()		
							.AutoHeight()
							[
								SNew(SULobbyMatchSetupPanel)
								.PlayerOwner(PlayerOwner)
								.MatchInfo(PlayerState->CurrentMatch)
								.bIsHost(PlayerState->CurrentMatch->OwnersPlayerState == PlayerState)
							]
						]
					]			
				];
		}
	}
}

void SULobbyInfoPanel::TickNonChatPanel(float DeltaTime)
{
	bool bForceMatchPanelRefresh = false;

	// Look to see if we need to display the match setup panel instead of the match dock
	AUTLobbyPlayerState* PlayerState = GetOwnerPlayerState();
	if (PlayerState && PlayerState->CurrentMatch && 
		 (PlayerState->CurrentMatch->CurrentState == ELobbyMatchState::WaitingForPlayers || PlayerState->CurrentMatch->CurrentState == ELobbyMatchState::Launching || PlayerState->CurrentMatch->CurrentState == ELobbyMatchState::Aborting) )
	{
		if (bShowingMatchDock)
		{
			bShowingMatchDock = false;
			BuildNonChatPanel();
		}
	}
	else
	{
		if (!bShowingMatchDock)
		{
			bForceMatchPanelRefresh = true;
			bShowingMatchDock = true;
			BuildNonChatPanel();
		}
	}

	if (bShowingMatchDock)
	{
		// We are showing the dock, look to see if new matches to join have been added to the queue and if they
		// have, add them

		AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
		if (LobbyGameState)
		{
			if (bForceMatchPanelRefresh || MatchData.Num() != LobbyGameState->AvailableMatches.Num())
			{
				MatchData.Empty();
				MatchPanelDock->ClearChildren();

				if (LobbyGameState->AvailableMatches.Num() >0)
				{
					for (int32 i=0;i<LobbyGameState->AvailableMatches.Num();i++)
					{
						TSharedPtr<SUMatchPanel> MP;
						TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo = LobbyGameState->AvailableMatches[i];
						MatchPanelDock->AddSlot()
						[
							SAssignNew(MP,SUMatchPanel)
							.MatchInfo(MatchInfo)
						];

						MatchData.Add( FMatchData(LobbyGameState->AvailableMatches[i], MP));
					}
				}
				else
				{
					MatchPanelDock->AddSlot()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Text( NSLOCTEXT("LobbyMatches","NoMatches","There are currently no matches, start one!"))
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.Action.TextStyle")
						]
					];
				}

			}
		}
	}
}



#endif