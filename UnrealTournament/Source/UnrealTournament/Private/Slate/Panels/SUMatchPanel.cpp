
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "UTGameEngine.h"
#include "UTLobbyGameState.h"
#include "UTLobbyMatchInfo.h"
#include "SUMatchPanel.h"
#include "UTLobbyPC.h"

#if !UE_SERVER

void SUMatchPanel::Construct(const FArguments& InArgs)
{
	bIsFeatured = InArgs._bIsFeatured;
	MatchInfo = InArgs._MatchInfo;
	OnClicked = InArgs._OnClicked;

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SBox)						// First the overlaid box that controls everything....
		.HeightOverride(192)
		.WidthOverride(192)
		[
			SNew(SButton)
			.OnClicked(this, &SUMatchPanel::ButtonClicked)
			.ButtonStyle(SUWindowsStyle::Get(),"UWindows.Lobby.MatchButton")
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(ButtonSurface, SVerticalBox)
				]
				+SVerticalBox::Slot()
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Fill)
				.AutoHeight()
				.Padding(20.0f,0.0f,20.0f,5.0f)
				[
					SNew(SBox)
					.HeightOverride(24)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MenuBar.Background"))
						]
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							[
								SAssignNew(ActionText, STextBlock)
									.Text( this, &SUMatchPanel::GetButtonText)
									.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.Action.TextStyle")
							]
						]
					]
				]
			]
		]
	];
	UpdateMatchInfo();
}

FText SUMatchPanel::GetButtonText() const
{
	if (MatchInfo.IsValid())
	{
		return MatchInfo->GetActionText();
	}

	return NSLOCTEXT("SUMatchPanel","Seutp","Initializing...");
}

FText SUMatchPanel::GetPlayerOneText() const
{
	if (MatchInfo.IsValid())
	{
		if (MatchInfo->CurrentState == ELobbyMatchState::InProgress)
		{
			if (MatchInfo->PlayersInMatchInstance.Num() > 0)
			{
				return FText::FromString(MatchInfo->PlayersInMatchInstance[0].PlayerName);
			}
		}
		else if ( MatchInfo->Players.Num() > 0 && MatchInfo->Players[0] )
		{
			return FText::FromString(MatchInfo->Players[0]->PlayerName);	
		}

	}

	return NSLOCTEXT("Generic","Unknown","???");
}

FText SUMatchPanel::GetPlayerTwoText() const
{
	if (MatchInfo.IsValid())
	{
		if (MatchInfo->CurrentState == ELobbyMatchState::InProgress)
		{
			if (MatchInfo->PlayersInMatchInstance.Num() > 1)
			{
				return FText::FromString(MatchInfo->PlayersInMatchInstance[1].PlayerName);
			}
		}
		else if ( MatchInfo->Players.Num() > 1 && MatchInfo->Players[1] )
		{
			return FText::FromString(MatchInfo->Players[1]->PlayerName);	
		}
	}

	return NSLOCTEXT("Generic","Unknown","???");
}




void SUMatchPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	UpdateMatchInfo();
}

void SUMatchPanel::UpdateMatchInfo()
{
	if (MatchInfo.IsValid())
	{
		SetVisibility(EVisibility::All);

		// Build the match panel.

		ButtonSurface->ClearChildren();

		if (MatchInfo->CurrentState == ELobbyMatchState::InProgress)
		{
			FText PlayerOneText = GetPlayerOneText();
			FText PlayerTwoText = GetPlayerTwoText();

			FText PlayerOneScore = (MatchInfo->PlayersInMatchInstance.Num() > 0) ? FText::AsNumber(MatchInfo->PlayersInMatchInstance[0].PlayerScore) : FText::AsNumber(0);
			FText PlayerTwoScore = (MatchInfo->PlayersInMatchInstance.Num() > 1) ? FText::AsNumber(MatchInfo->PlayersInMatchInstance[1].PlayerScore) : FText::GetEmpty();

			ButtonSurface->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text( PlayerOneText )
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.TextStyle")
				];

			ButtonSurface->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text( PlayerOneScore )
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.TextStyle")
				];

			ButtonSurface->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text( NSLOCTEXT("Genmeric","VS","-VS-"))
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.TextStyle")
				];

			ButtonSurface->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text( PlayerTwoText )
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.TextStyle")
				];

			ButtonSurface->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text( PlayerTwoScore )
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.TextStyle")
				];
		
		}
		else if (MatchInfo->CurrentState == ELobbyMatchState::WaitingForPlayers || MatchInfo->CurrentState == ELobbyMatchState::Launching)
		{
			ButtonSurface->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text( GetPlayerOneText() )
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.TextStyle")
				];


			ButtonSurface->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text( NSLOCTEXT("Genmeric","VS","-VS-"))
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.TextStyle")
				];

			ButtonSurface->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text( GetPlayerTwoText() )
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Lobby.MatchButton.TextStyle")
				];
		}
	}
	else
	{
		SetVisibility(EVisibility::Hidden);
	}

}

FReply SUMatchPanel::ButtonClicked()
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(GWorld, 0));
	if (LocalPlayer)
	{
		AUTLobbyPC* PC = Cast<AUTLobbyPC>(LocalPlayer->PlayerController);
		if (PC)
		{
			AUTLobbyPlayerState* PS = Cast<AUTLobbyPlayerState>(PC->PlayerState);
			{
				if(PS)
				{
					PS->ServerJoinMatch(MatchInfo.Get());
				}
			}
		}
	}
	return FReply::Handled();
}
#endif