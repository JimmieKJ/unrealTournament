
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../SUWScaleBox.h"
#include "Slate/SlateGameResources.h"
#include "SUHomePanel.h"



#if !UE_SERVER

void SUHomePanel::ConstructPanel(FVector2D ViewportSize)
{
	this->ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
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
						.Image(SUWindowsStyle::Get().GetBrush("NewSchool.Background"))
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
					.bMaintainAspectRatio(true)
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UT15.Logo.Overlay"))
					]
				]
			]
		]
	];

}


#endif