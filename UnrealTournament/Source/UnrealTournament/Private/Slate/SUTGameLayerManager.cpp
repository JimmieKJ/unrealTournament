// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUTGameLayerManager.h"

#if !UE_SERVER

#include "SPopup.h"
#include "STooltipPresenter.h"

void SUTGameLayerManager::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SOverlay)
		
		+ SOverlay::Slot()
		[
			SAssignNew(GameLayers, SOverlay)
		]

		+ SOverlay::Slot()
		[
			SNew(SPopup)
			[
				SAssignNew(TooltipPresenter, STooltipPresenter)
			]
		]
	];
}

bool SUTGameLayerManager::OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent)
{
	TooltipPresenter->SetContent(TooltipContent.IsValid() ? TooltipContent.ToSharedRef() : SNullWidget::NullWidget);

	return true;
}

void SUTGameLayerManager::AddLayer(TSharedRef<class SWidget> ViewportContent, const int32 ZOrder)
{
	GameLayers->AddSlot(ZOrder)
	[
		ViewportContent
	];
}

void SUTGameLayerManager::RemoveLayer(TSharedRef<class SWidget> ViewportContent)
{
	GameLayers->RemoveSlot(ViewportContent);
}

#endif