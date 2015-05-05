
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../SUWScaleBox.h"
#include "SlateExtras.h"
#include "Slate/SlateGameResources.h"
#include "SUTWebBrowserPanel.h"
#include "Engine/UserInterfaceSettings.h"

#if !UE_SERVER

void SUTWebBrowserPanel::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner)
{
	ShowControls = InArgs._ShowControls;

	SUWPanel::Construct(SUWPanel::FArguments(), InPlayerOwner);
}

void SUTWebBrowserPanel::ConstructPanel(FVector2D ViewportSize)
{
	this->ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SDPIScaler)
		.DPIScale(this, &SUTWebBrowserPanel::GetReverseScale)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(WebBrowserContainer, SVerticalBox)
			]
		]
	];
}

void SUTWebBrowserPanel::Browse(FString URL)
{
	if (WebBrowserContainer.IsValid())
	{
		WebBrowserContainer->ClearChildren();
		WebBrowserContainer->AddSlot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(WebBrowserPanel, SWebBrowser)
			.InitialURL(URL)
			.ShowControls(true)
		];
	}
}

float SUTWebBrowserPanel::GetReverseScale() const
{
	if (PlayerOwner.IsValid() && PlayerOwner->ViewportClient)
	{
		FVector2D ViewportSize;
		PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);
		return 1.0f / GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));
	}
	return 1.0f;
}


#endif