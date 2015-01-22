// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PhATModule.h"
#include "PhAT.h"
#include "PreviewScene.h"
#include "Editor/UnrealEd/Public/SViewportToolBar.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "PhATPreviewViewportClient.h"
#include "SPhATPreviewToolbar.h"
#include "SPhATPreviewViewport.h"
#include "SDockTab.h"


SPhATPreviewViewport::~SPhATPreviewViewport()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport->Destroy();
		ViewportClient->Viewport = NULL;
	}
}

void SPhATPreviewViewport::SetViewportType(ELevelViewportType ViewType)
{
	ViewportClient->SetViewportType(ViewType);
}

void SPhATPreviewViewport::Construct(const FArguments& InArgs)
{
	PhATPtr = InArgs._PhAT;

	this->ChildSlot
	[
		SAssignNew(ViewportWidget, SViewport)
		.EnableGammaCorrection(false)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
		.ShowEffectWhenDisabled(false)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			.VAlign(VAlign_Top)
			[
				SNew(SPhATPreviewViewportToolBar)
				.PhATPtr(PhATPtr)
		 		.Visibility(this, &SPhATPreviewViewport::GetWidgetVisibility)
				.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
			]
		]
	];
	
	ViewportClient = MakeShareable(new FPhATEdPreviewViewportClient(PhATPtr, PhATPtr.Pin()->GetSharedData()));

	ViewportClient->bSetListenerPosition = false;

	if (!FPhAT::IsPIERunning())
	{
		ViewportClient->SetRealtime(true);
	}

	ViewportClient->VisibilityDelegate.BindSP(this, &SPhATPreviewViewport::IsVisible);

	Viewport = MakeShareable(new FSceneViewport(ViewportClient.Get(), ViewportWidget));
	ViewportClient->Viewport = Viewport.Get();

	// The viewport widget needs an interface so it knows what should render
	ViewportWidget->SetViewportInterface(Viewport.ToSharedRef());
}

void SPhATPreviewViewport::RefreshViewport()
{
	Viewport->Invalidate();
	Viewport->InvalidateDisplay();
}

void SPhATPreviewViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	ViewportClient->GetScene()->GetWorld()->Tick(LEVELTICK_All, InDeltaTime);
}

bool SPhATPreviewViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground());
}

EVisibility SPhATPreviewViewport::GetWidgetVisibility() const
{
	return IsVisible()? EVisibility::Visible: EVisibility::Collapsed;
}

TSharedPtr<FSceneViewport> SPhATPreviewViewport::GetViewport() const
{
	return Viewport;
}

TSharedPtr<FPhATEdPreviewViewportClient> SPhATPreviewViewport::GetViewportClient() const
{
	return ViewportClient;
}

TSharedPtr<SViewport> SPhATPreviewViewport::GetViewportWidget() const
{
	return ViewportWidget;
}
