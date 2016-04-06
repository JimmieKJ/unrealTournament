// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTSpawnWindow.h"
#include "UTPlayerState.h"
#include "SUTButton.h"
#include "UTGameMode.h"

#include "../SUTStyle.h"

#if !UE_SERVER

void SUTSpawnWindow::BuildWindow()
{
	SUTHUDWindow::BuildWindow();
	BuildSpawnBar(920);
}

void SUTSpawnWindow::BuildSpawnBar(float YPosition)
{
	float BarHeight = 100.0f;
	Overlay->AddSlot().VAlign(VAlign_Fill).HAlign(HAlign_Fill)
	[
		SNew(SCanvas)
		+SCanvas::Slot()
		.Position(FVector2D(0,YPosition))
		.Size(FVector2D(1920.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SImage)
				.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.White"))
			]
		]

		+SCanvas::Slot()
		.Position(FVector2D(0,YPosition + 2.0f))
		.Size(FVector2D(1920.0f, BarHeight))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SBorder)
				.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Shaded"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f,0.0f,10.0f,0.0f)
					[
						SNew(SBox).WidthOverride(264.0f).HeightOverride(86.0f)
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.Logo.Small"))
						]
					]
					+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f,5.0f,0.0f,5.0f)
					[
						SNew(SBox).WidthOverride(2.0f).HeightOverride(BarHeight * 0.8f)
						[
							SNew(SImage)
							.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.White"))
						]
					]
					+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f,0.0f,10.0f,0.0f)
					[
						SNew(STextBlock)
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large.Bold")
						.Text(this, &SUTSpawnWindow::GetSpawnBarText)
					]
				]
			]
		]

		+SCanvas::Slot()
		.Position(FVector2D(0,YPosition + 2 + BarHeight))
		.Size(FVector2D(1920.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SImage)
				.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.White"))
			]
		]

	];
}

FText SUTSpawnWindow::GetSpawnBarText() const
{
/*
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->MyHUD)
	{
		AUTHUD* HUD = Cast<AUTHUD>(PlayerOwner->PlayerController->MyHUD);
		if (HUD)
		{
			bool bVisingMessage = false;
			return HUD->GetSpectatorMessageText(bVisingMessage);
		}
	}
*/
	return FText::GetEmpty();
}


#endif