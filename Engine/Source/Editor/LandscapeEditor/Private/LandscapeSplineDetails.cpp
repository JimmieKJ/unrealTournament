// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorPrivatePCH.h"
#include "LandscapeSplineDetails.h"
#include "LandscapeEdMode.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "LandscapeSplineDetails"


TSharedRef<IDetailCustomization> FLandscapeSplineDetails::MakeInstance()
{
	return MakeShareable(new FLandscapeSplineDetails);
}

void FLandscapeSplineDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& LandscapeSplineCategory = DetailBuilder.EditCategory("LandscapeSpline", FText::GetEmpty(), ECategoryPriority::Transform);

	LandscapeSplineCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0, 0, 2, 0)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectAll", "Select all connected:"))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SButton)
			.Text(LOCTEXT("ControlPoints", "Control Points"))
			.HAlign(HAlign_Center)
			.OnClicked(this, &FLandscapeSplineDetails::OnSelectConnectedControlPointsButtonClicked)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SButton)
			.Text(LOCTEXT("Segments", "Segments"))
			.HAlign(HAlign_Center)
			.OnClicked(this, &FLandscapeSplineDetails::OnSelectConnectedSegmentsButtonClicked)
		]
	];

	LandscapeSplineCategory.AddCustomRow(FText::GetEmpty())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0, 0, 2, 0)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SButton)
			.Text(LOCTEXT("Move Selected ControlPnts+Segs to Current level", "Move to current level"))
			.HAlign(HAlign_Center)
			.OnClicked(this, &FLandscapeSplineDetails::OnMoveToCurrentLevelButtonClicked)
			.IsEnabled(this, &FLandscapeSplineDetails::IsMoveToCurrentLevelButtonEnabled)
		]
	];
}

class FEdModeLandscape* FLandscapeSplineDetails::GetEditorMode() const
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

FReply FLandscapeSplineDetails::OnSelectConnectedControlPointsButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		LandscapeEdMode->SelectAllConnectedSplineControlPoints();
	}

	return FReply::Handled();
}

FReply FLandscapeSplineDetails::OnSelectConnectedSegmentsButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		LandscapeEdMode->SelectAllConnectedSplineSegments();
	}

	return FReply::Handled();
}

FReply FLandscapeSplineDetails::OnMoveToCurrentLevelButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid() && LandscapeEdMode->CurrentToolTarget.LandscapeInfo->GetCurrentLevelLandscapeProxy(true))
	{
		LandscapeEdMode->SplineMoveToCurrentLevel();
	}

	return FReply::Handled();
}

bool FLandscapeSplineDetails::IsMoveToCurrentLevelButtonEnabled() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	return (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid() && LandscapeEdMode->CurrentToolTarget.LandscapeInfo->GetCurrentLevelLandscapeProxy(true));
}

#undef LOCTEXT_NAMESPACE
