// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "Widgets/SUTAspectPanel.h"
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
			SNew(SUTAspectPanel)
			.Visibility(EVisibility::SelfHitTestInvisible)
			[
				SAssignNew(GameLayers, SOverlay)
			]
		]

		+ SOverlay::Slot()
		[
			SAssignNew(GameLayers_NoAspect, SOverlay)
		]

		+ SOverlay::Slot()
		[
			SNew(SUTAspectPanel)
			.Visibility(EVisibility::SelfHitTestInvisible)
			[
				SNew(SPopup)
				[
					SAssignNew(TooltipPresenter, STooltipPresenter)
				]
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

void SUTGameLayerManager::AddLayer_NoAspect(TSharedRef<class SWidget> ViewportContent, const int32 ZOrder)
{
	GameLayers_NoAspect->AddSlot(ZOrder)
		[
			ViewportContent
		];
}

void SUTGameLayerManager::RemoveLayer_NoAspect(TSharedRef<class SWidget> ViewportContent)
{
	GameLayers_NoAspect->RemoveSlot(ViewportContent);
}

#endif