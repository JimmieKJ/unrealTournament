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
					SAssignNew(CreditsWebBrowser, SWebBrowser)
					.InitialURL(TEXT("http://epic.gm/launchercontribs"))
					.ShowControls(false)
				]
			]
		]
	];
}

void SUWCreditsPanel::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	// Have to hack around SWebBrowser not allowing url changes
	WebBrowserBox->RemoveSlot(CreditsWebBrowser.ToSharedRef());
	WebBrowserBox->AddSlot()
	[
		SAssignNew(CreditsWebBrowser, SWebBrowser)
		.InitialURL(TEXT("http://epic.gm/launchercontribs"))
		.ShowControls(false)
	];
}

#endif