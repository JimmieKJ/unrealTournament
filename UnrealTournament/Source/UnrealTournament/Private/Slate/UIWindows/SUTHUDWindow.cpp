// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTHUDWindow.h"

#if !UE_SERVER

void SUTHUDWindow::BuildWindow()
{
	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SAssignNew(Overlay, SOverlay)
		
		// Shadowed background
		+SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).HeightOverride(1080.0f).WidthOverride(1920.0f)
					[
						SAssignNew(Canvas, SCanvas)						
					]
				]
			]
		]
	];
}

bool SUTHUDWindow::CanWindowClose()
{
	return true;
}


#endif