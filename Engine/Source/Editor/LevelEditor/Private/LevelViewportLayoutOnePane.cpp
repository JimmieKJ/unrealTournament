// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelEditor.h"
#include "LevelViewportLayoutOnePane.h"
#include "SLevelViewport.h"

// FLevelViewportLayoutOnePane /////////////////////////////

void FLevelViewportLayoutOnePane::SaveLayoutString(const FString& LayoutString) const
{
	if (!bIsTransitioning)
	{
		FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

		SaveCommonLayoutString(SpecificLayoutString);
	}
}

TSharedRef<SWidget> FLevelViewportLayoutOnePane::MakeViewportLayout(const FString& LayoutString)
{
	// single viewport layout blocks maximize feature as it doesn't make sense
	bIsMaximizeSupported = false;

	FString SpecificLayoutString = GetTypeSpecificLayoutString(LayoutString);

	FEngineShowFlags OrthoShowFlags(ESFIM_Editor);
	ApplyViewMode(VMI_BrushWireframe, false, OrthoShowFlags);

	FEngineShowFlags PerspectiveShowFlags(ESFIM_Editor);
	ApplyViewMode(VMI_Lit, true, PerspectiveShowFlags);

	FString ViewportKey;
	if (!SpecificLayoutString.IsEmpty())
	{
		ViewportKey = SpecificLayoutString + TEXT(".Viewport0");
	}

	TSharedPtr<SLevelViewport> ViewportWidget;

	ViewportBox =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SAssignNew(ViewportWidget, SLevelViewport)
			.ViewportType(LVT_Perspective)
			.Realtime(true)
			.ParentLayout(AsShared())
			.ParentLevelEditor(ParentLevelEditor)
			.ConfigKey(ViewportKey)
		];

	Viewports.Add(ViewportWidget);

	// Make newly-created perspective viewports active by default
	GCurrentLevelEditingViewportClient = &ViewportWidget->GetLevelViewportClient();

	InitCommonLayoutFromString(SpecificLayoutString);

	return StaticCastSharedRef<SWidget>(ViewportBox.ToSharedRef());
}

void FLevelViewportLayoutOnePane::ReplaceWidget(TSharedRef<SWidget> Source, TSharedRef<SWidget> Replacement)
{
	check(ViewportBox->GetChildren()->Num() == 1)
	TSharedRef<SWidget> ViewportWidget = ViewportBox->GetChildren()->GetChildAt(0);

	check(ViewportWidget == Source);
	ViewportBox->RemoveSlot(Source);
	ViewportBox->AddSlot()
		[
			Replacement
		];
}
