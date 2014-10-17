
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
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("Test","Test","Test"))
								.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MOTD.ServerTitle")
							]
						]
					]			
			];
	}

}

#endif