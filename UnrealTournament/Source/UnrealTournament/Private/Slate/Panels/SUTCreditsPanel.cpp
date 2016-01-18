// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTCreditsPanel.h"

#if !UE_SERVER

void SUTCreditsPanel::ConstructPanel(FVector2D ViewportSize)
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

void SUTCreditsPanel::OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow)
{
	SUTPanelBase::OnShowPanel(inParentWindow);
	if (CreditsWebBrowser.IsValid())
	{
		CreditsWebBrowser->Browse(TEXT("http://epic.gm/utcontrib"));
	}
}

#endif