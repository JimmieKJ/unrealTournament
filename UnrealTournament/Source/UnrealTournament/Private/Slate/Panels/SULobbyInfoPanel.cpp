
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "Slate.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "UTGameEngine.h"
#include "UTLobbyGameState.h"
#include "SUMidGameInfoPanel.h"
#include "SULobbyInfoPanel.h"
#include "SUMatchPanel.h"


#if !UE_SERVER


void SULobbyInfoPanel::BuildPage(FVector2D ViewportSize)
{
	SUMidGameInfoPanel::BuildPage(ViewportSize);
}

void SULobbyInfoPanel::AddInfoPanel()
{
	if (InfoPanelOverlay.IsValid())
	{
		InfoPanelOverlay->AddSlot()
			[
				SAssignNew(InfoPanel, SVerticalBox)
				+SVerticalBox::Slot()		
					.AutoHeight()
					.VAlign(VAlign_Top)
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
							+SVerticalBox::Slot()		// Server Title
							.AutoHeight()
							[
								SNew(SScrollBox)
								.Orientation(Orient_Horizontal)
								+SScrollBox::Slot()
								[
									SAssignNew(MatchPanels, SHorizontalBox)
									+SHorizontalBox::Slot()
									.Padding(5.0f,0.0f,5.0f,0.0f)
									[
										SNew(SVerticalBox)
										+SVerticalBox::Slot()
										.HAlign(HAlign_Center)
										[
											SNew(STextBlock)
											.Text( NSLOCTEXT("LobbyMatches","NoMatches","There are currently no matches, start one!"))
											.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.Action.TextStyle")
										]
									]
								]
							]
						]
					]			
			];
	}

}

void SULobbyInfoPanel::OnShowPanel()
{

}

void SULobbyInfoPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SUMidGameInfoPanel::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	AUTLobbyGameState* GameState = GWorld->GetGameState<AUTLobbyGameState>();

	if (GameState)
	{
		int32 Num;
		FChildren* Children = MatchPanels->GetChildren();
		Num = Children->Num();

		if (Num != GameState->AvailableMatches.Num())
		{
			MatchPanels->ClearChildren();
			for (int32 i=0;i<GameState->AvailableMatches.Num();i++)
			{
				MatchPanels->AddSlot()
				[
					SNew(SUMatchPanel)
					.MatchInfo(GameState->AvailableMatches[i])
				];
			}
		}
	}
}


#endif