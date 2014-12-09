
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

void SULobbyInfoPanel::ConstructPanel(FVector2D ViewportSize)
{
	SUChatPanel::ConstructPanel(ViewportSize);
	Tag = FName(TEXT("LobbyInfoPanel"));

	LastMatchState = ELobbyMatchState::Setup;

	AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(GetOwnerPlayerState());
	PlayerState->CurrentMatchChangedDelegate.BindSP(this, &SULobbyInfoPanel::OwnerCurrentMatchChanged);
	bShowingMatchDock = PlayerState->CurrentMatch == NULL;
	BuildNonChatPanel();
}

void SULobbyInfoPanel::BuildNonChatPanel()
{
	// Clear out any existing panels
	NonChatPanel->ClearChildren();

	AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(GetOwnerPlayerState());
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
			if (PlayerState->CurrentMatch->CurrentState == ELobbyMatchState::Initializing || PlayerState->CurrentMatch->CurrentState == ELobbyMatchState::Setup)
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
									SNew(STextBlock)
									.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.NormalText")
									.Text(NSLOCTEXT("LobbyMessages","Initializing","Recieving Match Settings from Server..."))
								]
							]
						]			
					];
			}
			else
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
									.bIsHost(PlayerState->CurrentMatch->OwnerId == PlayerState->UniqueId)
								]
							]
						]			
					];

			}
		}
	}
}

bool SULobbyInfoPanel::AlreadyTrackingMatch(AUTLobbyMatchInfo* TestMatch)
{
	for (int32 i=0;i<MatchData.Num();i++)
	{
		if (MatchData[i].Get()->Info == TestMatch)
		{
			return true;
		}
	}

	return false;

}

void SULobbyInfoPanel::TickNonChatPanel(float DeltaTime)
{
	if (bShowingMatchDock)
	{
		// We are showing the dock, look to see if new matches to join have been added to the queue and if they
		// have, add them

		AUTLobbyGameState* LobbyGameState = GWorld->GetGameState<AUTLobbyGameState>();
		if (LobbyGameState)
		{

			bool bDockRequiresUpdate = false;

			// Expire out any old matches that are no longer useful.

			for (int32 i=0;i<MatchData.Num();i++)
			{
				if (MatchData[i]->Info.IsValid())
				{
					if (!LobbyGameState->IsMatchStillValid(MatchData[i]->Info.Get()) || !MatchData[i]->Info->ShouldShowInDock())
					{
						bDockRequiresUpdate = true;
					}
				}
			}

			// Look for any matches not in the dock
			for (int32 i=0;i<LobbyGameState->AvailableMatches.Num();i++)
			{
				AUTLobbyMatchInfo* Match = LobbyGameState->AvailableMatches[i];
				if (Match && Match->CurrentState != ELobbyMatchState::Dead && Match->CurrentState != ELobbyMatchState::Recycling)
				{
					if (!AlreadyTrackingMatch(LobbyGameState->AvailableMatches[i]))
					{
						bDockRequiresUpdate = true;
					}
				}
			}

			if (bDockRequiresUpdate)
			{
				MatchData.Empty();
				MatchPanelDock->ClearChildren();

				if (LobbyGameState->AvailableMatches.Num() >0)
				{
					for (int32 i=0;i<LobbyGameState->AvailableMatches.Num();i++)
					{
						if (LobbyGameState->AvailableMatches[i] && LobbyGameState->AvailableMatches[i]->ShouldShowInDock())
						{
							TSharedPtr<SUMatchPanel> MP;
							TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo = LobbyGameState->AvailableMatches[i];
							MatchPanelDock->AddSlot()
							[
								SAssignNew(MP,SUMatchPanel)
								.MatchInfo(MatchInfo)
							];

							MatchData.Add( FMatchData::Make(LobbyGameState->AvailableMatches[i], MP));
						}
					}
				}
				else
				{
					BuildEmptyMatchMessage();
				}

			}
		}
	}
	else
	{
		AUTLobbyPlayerState* PlayerState = Cast<AUTLobbyPlayerState>(GetOwnerPlayerState());
		if (PlayerState && PlayerState->CurrentMatch->CurrentState != LastMatchState)
		{
			BuildNonChatPanel();
			LastMatchState = PlayerState->CurrentMatch->CurrentState;
		}
	
	}
}

void SULobbyInfoPanel::BuildEmptyMatchMessage()
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
void SULobbyInfoPanel::OwnerCurrentMatchChanged(AUTLobbyPlayerState* LobbyPlayerState)
{
	if (LobbyPlayerState->CurrentMatch)
	{
		// We are in a match, either hosting or via a join, so hide the match doc.

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
			bShowingMatchDock = true;
			MatchData.Empty();
			BuildNonChatPanel();
			BuildEmptyMatchMessage();
		}
	}
}

#endif