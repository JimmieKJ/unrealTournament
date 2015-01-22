// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelsPrivatePCH.h"
#include "SLevelsViewRow.h"
#include "SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "LevelsView"


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef< SWidget > SLevelsViewRow::GenerateWidgetForColumn(const FName& ColumnID)
{
	TSharedPtr< SWidget > TableRowContent;

	if (ColumnID == LevelsView::ColumnID_LevelLabel)
	{
		TableRowContent =
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(this, &SLevelsViewRow::GetFont)
				.Text(ViewModel.Get(), &FLevelViewModel::GetDisplayName)
				.ColorAndOpacity(this, &SLevelsViewRow::GetColorAndOpacity)
				.HighlightText(HighlightText)
				.ToolTipText(LOCTEXT("DoubleClickToolTip", "Double-Click to make this the current Level"))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
			]
		;
	}
	else if (ColumnID == LevelsView::ColumnID_ActorCount)
	{
		TableRowContent =
			SNew(SHorizontalBox)
			.Visibility(this, &SLevelsViewRow::IsActorColumnVisible)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(this, &SLevelsViewRow::GetFont)
				.Text(ViewModel.Get(), &FLevelViewModel::GetActorCountString)
				.ColorAndOpacity(this, &SLevelsViewRow::GetColorAndOpacity)
				.HighlightText(HighlightText)
				.ToolTipText(LOCTEXT("ActorCountToolTip", "The number of Actors in this level"))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
			]
		;
	}
	else if (ColumnID == LevelsView::ColumnID_LightmassSize)
	{
		TableRowContent =
			SNew(SHorizontalBox)
			.Visibility(this, &SLevelsViewRow::IsLightmassSizeColumnVisible)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(this, &SLevelsViewRow::GetFont)
				.Text(ViewModel.Get(), &FLevelViewModel::GetLightmassSizeString)
				.ColorAndOpacity(this, &SLevelsViewRow::GetColorAndOpacity)
				.HighlightText(HighlightText)
				.ToolTipText(LOCTEXT("LightmassSizeToolTip", "The size of the lightmap data for this level"))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
			]
		;
	}
	else if (ColumnID == LevelsView::ColumnID_FileSize)
	{
		TableRowContent =
			SNew(SHorizontalBox)
			.Visibility(this, &SLevelsViewRow::IsFileSizeColumnVisible)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(this, &SLevelsViewRow::GetFont)
				.Text(ViewModel.Get(), &FLevelViewModel::GetFileSizeString)
				.ColorAndOpacity(this, &SLevelsViewRow::GetColorAndOpacity)
				.HighlightText(HighlightText)
				.ToolTipText(LOCTEXT("FileSizeToolTip", "The size of the file for this level"))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
			]
		;
	}
	else if (ColumnID == LevelsView::ColumnID_Visibility)
	{
		TableRowContent =
			SAssignNew(VisibilityButton, SButton)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.OnClicked(this, &SLevelsViewRow::OnToggleVisibility)
			.ToolTipText(LOCTEXT("VisibilityButtonToolTip", "Toggle Level Visibility"))
			.ForegroundColor(FSlateColor::UseForeground())
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew(SImage)
				.Image(this, &SLevelsViewRow::GetVisibilityBrushForLevel)
				.ColorAndOpacity(this, &SLevelsViewRow::GetForegroundColorForVisibilityButton)
			]
		;
	}
	else if (ColumnID == LevelsView::ColumnID_Lock)
	{
		TableRowContent =
			SAssignNew(LockButton, SButton)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.OnClicked(this, &SLevelsViewRow::OnToggleLock)
			.ToolTipText(this, &SLevelsViewRow::GetLockToolTipForLevel)
			.ForegroundColor(FSlateColor::UseForeground())
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew(SImage)
				.Image(this, &SLevelsViewRow::GetLockBrushForLevel)
				.ColorAndOpacity(this, &SLevelsViewRow::GetForegroundColorForLockButton)
			]
		;
	}
	else if (ColumnID == LevelsView::ColumnID_Color)
	{
		TableRowContent =
			SAssignNew(ColorButton, SButton)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.OnClicked(this, &SLevelsViewRow::OnChangeColor)
			.ToolTipText(LOCTEXT("ColorButtonToolTip", "Change Level Color"))
			.ForegroundColor(FSlateColor::UseForeground())
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew(SImage)
				.Image(this, &SLevelsViewRow::GetColorBrushForLevel)
				.ColorAndOpacity(TAttribute< FSlateColor >::Create(TAttribute< FSlateColor >::FGetter::CreateSP(this, &SLevelsViewRow::GetLevelColorAndOpacity)))
			]
		;
	}
	else if (ColumnID == LevelsView::ColumnID_SCCStatus)
	{
		TableRowContent =
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(this, &SLevelsViewRow::GetSCCStateImage)
					.ToolTipText(this, &SLevelsViewRow::GetSCCStateTooltip)
				]
			]
		;
	}
	else if (ColumnID == LevelsView::ColumnID_Save)
	{
		TableRowContent =
			SAssignNew(SaveButton, SButton)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.OnClicked(this, &SLevelsViewRow::OnSave)
			.ToolTipText(LOCTEXT("SaveButtonToolTip", "Save Level"))
			.ForegroundColor(FSlateColor::UseForeground())
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew(SImage)
				.Image(this, &SLevelsViewRow::GetSaveBrushForLevel)
				.ColorAndOpacity(TAttribute< FSlateColor >::Create(TAttribute< FSlateColor >::FGetter::CreateSP(this, &SLevelsViewRow::GetSaveButtonColorAndOpacity)))
			]
		;
	}
	else if (ColumnID == LevelsView::ColumnID_Kismet)
	{
		TableRowContent =
			SAssignNew(KismetButton, SButton)
			.ContentPadding(0)
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.OnClicked(this, &SLevelsViewRow::OnOpenKismet)
			.ToolTipText(LOCTEXT("KismetButtonToolTip", "Open Level Blueprint"))
			.ForegroundColor(FSlateColor::UseForeground())
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew(SImage)
				.Image(this, &SLevelsViewRow::GetKismetBrushForLevel)
				.ColorAndOpacity(this, &SLevelsViewRow::GetForegroundColorForKismetButton)
			]
		;
	}
	else if (ColumnID == LevelsView::ColumnID_EditorOffset)
	{
		TableRowContent =
			SNew(SHorizontalBox)
			.Visibility(this, &SLevelsViewRow::IsEditorOffsetColumnVisible)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(10, 0, 0, 0))
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.FillWidth(0.25f)
			[
				SNew(SButton)
				.Text(LOCTEXT("EditLevelTransform", "Viewport Edit"))
				.ToolTipText(LOCTEXT("EditLevelToolTip", "Edit level transform in viewport."))
				.OnClicked(FOnClicked::CreateSP(this, &SLevelsViewRow::OnEditLevelClicked))
				.IsEnabled(this, &SLevelsViewRow::LevelTransformAllowed)
			]
		+ SHorizontalBox::Slot()
			.Padding(FMargin(10, 0, 0, 0))
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.FillWidth(0.55f)
			[
				SNew(SVectorInputBox)
				.X(this, &SLevelsViewRow::GetLevelOffset, 0)
				.Y(this, &SLevelsViewRow::GetLevelOffset, 1)
				.Z(this, &SLevelsViewRow::GetLevelOffset, 2)
				.bColorAxisLabels(true)
				.OnXCommitted(this, &SLevelsViewRow::OnSetLevelOffset, 0)
				.OnYCommitted(this, &SLevelsViewRow::OnSetLevelOffset, 1)
				.OnZCommitted(this, &SLevelsViewRow::OnSetLevelOffset, 2)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"))) // Look the same as transform editing in the details panel
				.IsEnabled(this, &SLevelsViewRow::LevelEditTextTransformAllowed)
			]
		+ SHorizontalBox::Slot()
			.Padding(FMargin(10, 0, 0, 0))
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.FillWidth(0.2f)
			[
				SNew(SNumericEntryBox<float>)
				.IsEnabled(this, &SLevelsViewRow::LevelEditTextTransformAllowed)
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Delta(90.0f)
				.AllowSpin(true)
				.MinValue(0.0f)
				.MaxValue(270.0f)
				.MinSliderValue(0.0f)
				.MaxSliderValue(270.0f)
				.Value(this, &SLevelsViewRow::GetLevelRotation)
				.OnValueChanged(this, &SLevelsViewRow::OnSetLevelRotation)
				.OnValueCommitted(this, &SLevelsViewRow::OnCommitLevelRotation)
				.OnBeginSliderMovement(this, &SLevelsViewRow::OnBeginLevelRotatonSlider)
				.OnEndSliderMovement(this, &SLevelsViewRow::OnEndLevelRotatonSlider)
				.LabelPadding(0)
				.Label()
				[
					SNumericEntryBox<float>::BuildLabel(LOCTEXT("LevelRotation_Label", "Yaw"), FLinearColor::White, SNumericEntryBox<float>::BlueLabelBackgroundColor)
				]
			]
		;
	}
	else
	{
		checkf(false, TEXT("Unknown ColumnID provided to SLevelsView"));
	}

	return TableRowContent.ToSharedRef();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
