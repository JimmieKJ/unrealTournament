// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUWCreditsPanel.h"

#if !UE_SERVER

void SUWCreditsPanel::ConstructPanel(FVector2D ViewportSize)
{
	Tag = FName(TEXT("CreditsPanel"));
	
	this->ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(WebBrowserBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SAssignNew(CreditsWebBrowser, SUTWebBrowserPanel, PlayerOwner)
					.InitialURL(TEXT("http://epic.gm/utcontrib"))
					.ShowControls(false)
				]
			]
		]
	];
}

void SUWCreditsPanel::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	SUWPanel::OnShowPanel(inParentWindow);
	if (CreditsWebBrowser.IsValid())
	{
		CreditsWebBrowser->Browse(TEXT("http://epic.gm/utcontrib"));
	}
}

#endif