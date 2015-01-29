// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsMidGame.h"
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

void SUWindowsMidGame::BuildDesktop()
{
	if (!DesktopPanel.IsValid())
	{
		SAssignNew(DesktopPanel, SUMidGameInfoPanel)
			.PlayerOwner(PlayerOwner);
	}

	if (DesktopPanel.IsValid())
	{
		ActivatePanel(DesktopPanel);
	}
}

void SUWindowsMidGame::BuildTopBar()
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
					SAssignNew(LeftStatusText, STextBlock)
					.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Status.TextStyle"))
				]

				+SHorizontalBox::Slot()
				.Padding(25,0,5,0)
				.HAlign(HAlign_Right)
				[
					SAssignNew(RightStatusText, STextBlock)
					.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Status.TextStyle"))
				]

				+SHorizontalBox::Slot()
				.Padding(0,0,5,10)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.OnClicked(this, &SUWindowsMidGame::Play)
					.ButtonStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Button"))
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("Gerneric","Play","ESC to Close"))
						.TextStyle(SUWindowsStyle::Get(), PlayerOwner->TeamStyleRef("UWindows.MidGameMenu.Status.TextStyle"))
					]
				]

			]
		];
	}
}

void SUWindowsMidGame::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	AUTGameState* GS = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();

	if (GS != NULL)
	{
		FText NewText;

		if (GS->bTeamGame)
		{
			int32 Players[2];
			int32 Specs = 0;

			Players[0] = 0;
			Players[1] = 0;

			for (int32 i=0;i<GS->PlayerArray.Num();i++)
			{
				if (GS->PlayerArray[i] != NULL)
				{
					AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
					if (PS != NULL)
					{
						if (PS->bIsSpectator)
						{
							Specs++;
						}
						else
						{
							Players[FMath::Clamp<int32>(PS->GetTeamNum(),0,1)]++;
						}
					}
				}
			}

			NewText = FText::Format(NSLOCTEXT("SUWindowsMidGame","TeamStatus","Red: {0}  Blue: {1}  Spectators {2}"), FText::AsNumber(Players[0]), FText::AsNumber(Players[1]), FText::AsNumber(Specs));
		}
		else
		{
			int32 Players = 0;
			int32 Specs = 0;
			for (int32 i=0;i<GS->PlayerArray.Num();i++)
			{
				if (GS->PlayerArray[i] != NULL)
				{
					AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
					if (PS != NULL)
					{
						if (PS->bIsSpectator)
						{
							Specs++;
						}
						else 
						{
							Players++;
						}
					}
				}
			}

			NewText = FText::Format(NSLOCTEXT("SUWindowsMidGame","Status","Players: {0} Spectators {1}"), FText::AsNumber(Players), FText::AsNumber(Specs));
		}

		LeftStatusText->SetText(ConvertTime(GS->ElapsedTime));
		RightStatusText->SetText(NewText);
	}

}


#endif