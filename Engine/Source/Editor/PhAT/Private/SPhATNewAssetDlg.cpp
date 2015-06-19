// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PhATModule.h"
#include "SPhATNewAssetDlg.h"
#include "SNumericEntryBox.h"
#include "STextComboBox.h"

// Add in the constants from the static mesh editor as we need them here too
const float MaxHullAccuracy = 1.f;
const float MinHullAccuracy = 0.f;
const float DefaultHullAccuracy = 0.5f;
const float HullAccuracyDelta = 0.01f;

const int32 MaxVertsPerHullCount = 32;
const int32 MinVertsPerHullCount = 6;
const int32 DefaultVertsPerHull = 16;

void SPhATNewAssetDlg::Construct(const FArguments& InArgs)
{
	NewBodyData = InArgs._NewBodyData.Get();
	NewBodyResponse = InArgs._NewBodyResponse.Get();
	ParentWindow = InArgs._ParentWindow.Get();
	ParentWindow.Pin()->SetWidgetToFocusOnActivate(SharedThis(this));

	check(NewBodyData && NewBodyResponse);

	// Initialize combobox options
	CollisionGeometryOptions.Add(TSharedPtr<FString>(new FString(TEXT("Sphyl"))));
	CollisionGeometryOptions.Add(TSharedPtr<FString>(new FString(TEXT("Sphere"))));
	CollisionGeometryOptions.Add(TSharedPtr<FString>(new FString(TEXT("Box"))));

	// Add in the ability to create a convex hull from the source geometry for skeletal meshes
	CollisionGeometryOptions.Add(TSharedPtr<FString>(new FString(TEXT("Single Convex Hull"))));
	CollisionGeometryOptions.Add(TSharedPtr<FString>(new FString(TEXT("Multi Convex Hull"))));

	WeightOptions.Add(TSharedPtr<FString>(new FString(TEXT("Dominant Weight"))));
	WeightOptions.Add(TSharedPtr<FString>(new FString(TEXT("Any Weight"))));

	AngularConstraintModes.Add(TSharedPtr<FString>(new FString(TEXT("Limited"))));
	AngularConstraintModes.Add(TSharedPtr<FString>(new FString(TEXT("Locked"))));
	AngularConstraintModes.Add(TSharedPtr<FString>(new FString(TEXT("Free"))));

	// Initialize new body parameters
	NewBodyData->Initialize();

	NewBodyData->HullAccuracy = DefaultHullAccuracy;
	NewBodyData->MaxHullVerts = DefaultVertsPerHull;

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(4.0f)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				// Option widgets
				SNew(SUniformGridPanel)
				+SUniformGridPanel::Slot(0, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "MinimumBoneSizeLabel", "Minimum Bone Size:"))
				]
				+SUniformGridPanel::Slot(1, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SNumericEntryBox<float>)
					.Value(this, &SPhATNewAssetDlg::GetMinBoneSize )
					.OnValueCommitted(this, &SPhATNewAssetDlg::OnMinimumBoneSizeCommitted)
				]
				+SUniformGridPanel::Slot(0, 1)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "OrientAlongBoneLabel", "Orient Along Bone:"))
				]
				+SUniformGridPanel::Slot(1, 1)
				[
					SNew(SCheckBox)
					.IsChecked(NewBodyData->bAlignDownBone ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SPhATNewAssetDlg::OnToggleOrientAlongBone)
				]
				+SUniformGridPanel::Slot(0, 2)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "CollisionGeometryLabel", "Collision Geometry:"))
				]
				+SUniformGridPanel::Slot(1, 2)
				.VAlign(VAlign_Center)
				[
					SNew(STextComboBox)
					.OptionsSource(&CollisionGeometryOptions)
					.InitiallySelectedItem(CollisionGeometryOptions[0])
					.OnSelectionChanged(this, &SPhATNewAssetDlg::OnCollisionGeometrySelectionChanged)
				]
				+SUniformGridPanel::Slot(0, 3)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "UseVertsWithLabel", "Use Verts With:"))
				]
				+SUniformGridPanel::Slot(1, 3)
				.VAlign(VAlign_Center)
				[
					SNew(STextComboBox)
					.OptionsSource(&WeightOptions)
					.InitiallySelectedItem(WeightOptions[0])
					.OnSelectionChanged(this, &SPhATNewAssetDlg::OnWeightOptionSelectionChanged)
				]
				+SUniformGridPanel::Slot(0, 4)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "CreateJointsLabel", "Create Joints:"))
				]
				+SUniformGridPanel::Slot(1, 4)
				[
					SNew(SCheckBox)
					.IsChecked(NewBodyData->bCreateJoints ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SPhATNewAssetDlg::OnToggleCreateJoints)
				]
				+SUniformGridPanel::Slot(0, 5)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "AngularConstraintMode", "Default Angular Constraint Mode:"))
				]
				+SUniformGridPanel::Slot(1, 5)
				.VAlign(VAlign_Center)
				[
					SNew(STextComboBox)
					.OptionsSource(&AngularConstraintModes)
					.InitiallySelectedItem(AngularConstraintModes[0])
					.OnSelectionChanged(this, &SPhATNewAssetDlg::OnAngularConstraintModeSelectionChanged)
				]
				+SUniformGridPanel::Slot(0, 6)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "WalkPastSmallBonesLabel", "Walk Past Small Bones:"))
				]
				+SUniformGridPanel::Slot(1, 6)
				[
					SNew(SCheckBox)
					.IsChecked(NewBodyData->bWalkPastSmall ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SPhATNewAssetDlg::OnToggleWalkPastSmallBones)
				]
				+SUniformGridPanel::Slot(0, 7)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("PhAT", "CreateBodyForAllBonesLabel", "Create Body For All Bones:"))
				]
				+SUniformGridPanel::Slot(1, 7)
				[
					SNew(SCheckBox)
					.IsChecked(NewBodyData->bBodyForAll ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
					.OnCheckStateChanged(this, &SPhATNewAssetDlg::OnToggleCreateBodyForAllBones)
				]
				// Add in the UI options for the accuracy and the max verts on the hulls
				+SUniformGridPanel::Slot(0, 7)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Visibility(this, &SPhATNewAssetDlg::GetHullOptionsVisibility )
						.Text( NSLOCTEXT("PhAT", "Accuracy_ConvexDecomp", "Accuracy") )
					]
				+SUniformGridPanel::Slot(1, 7)
					[
						SAssignNew(HullAccuracy, SSpinBox<float>)
						.Visibility(this, &SPhATNewAssetDlg::GetHullOptionsVisibility )
						.MinValue(MinHullAccuracy)
						.MaxValue(MaxHullAccuracy)
						.Delta(HullAccuracyDelta)
						.Value( this, &SPhATNewAssetDlg::GetHullAccuracy )
						.OnValueChanged( this, &SPhATNewAssetDlg::OnHullAccuracyChanged )
					]
				+SUniformGridPanel::Slot(0, 8)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Visibility(this, &SPhATNewAssetDlg::GetHullOptionsVisibility )
						.Text( NSLOCTEXT("PhAT", "MaxHullVerts_ConvexDecomp", "Max Hull Verts") )
					]
				+SUniformGridPanel::Slot(1, 8)
					[
						SAssignNew(MaxVertsPerHull, SSpinBox<int32>)
						.Visibility(this, &SPhATNewAssetDlg::GetHullOptionsVisibility )
						.MinValue(MinVertsPerHullCount)
						.MaxValue(MaxVertsPerHullCount)
						.Value( this, &SPhATNewAssetDlg::GetVertsPerHullCount )
						.OnValueChanged( this, &SPhATNewAssetDlg::OnVertsPerHullCountChanged )
					]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.Text(NSLOCTEXT("PhAT", "OkButtonText", "Ok"))
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(FOnClicked::CreateSP<SPhATNewAssetDlg, EAppReturnType::Type>(this, &SPhATNewAssetDlg::OnClicked, EAppReturnType::Ok))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.Text(NSLOCTEXT("PhAT", "CancelButtonText", "Cancel"))
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(FOnClicked::CreateSP<SPhATNewAssetDlg, EAppReturnType::Type>(this, &SPhATNewAssetDlg::OnClicked, EAppReturnType::Cancel))
				]
			]
		]
	];
}

FReply SPhATNewAssetDlg::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	//see if we pressed the Enter or Spacebar keys
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return OnClicked(EAppReturnType::Cancel);
	}

	//if it was some other button, ignore it
	return FReply::Unhandled();
}

bool SPhATNewAssetDlg::SupportsKeyboardFocus() const
{
	return true;
}

FReply SPhATNewAssetDlg::OnClicked(EAppReturnType::Type InResponse)
{
	*NewBodyResponse = InResponse;
	ParentWindow.Pin()->RequestDestroyWindow();
	return FReply::Handled();
}

void SPhATNewAssetDlg::OnMinimumBoneSizeCommitted(float NewValue, ETextCommit::Type CommitInfo)
{
	NewBodyData->MinBoneSize = NewValue;
}

TOptional<float> SPhATNewAssetDlg::GetMinBoneSize() const
{
	TOptional<float> MinBoneSize;
	MinBoneSize = NewBodyData->MinBoneSize;
	return MinBoneSize;
}

void SPhATNewAssetDlg::OnCollisionGeometrySelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (*NewSelection == TEXT("Box"))
	{
		NewBodyData->GeomType = EFG_Box;
	}
	else if (*NewSelection == TEXT("Sphere"))
	{
		NewBodyData->GeomType = EFG_Sphere;
	}
	// Check to see if the user has picked that they want to create a convex hull
	else if ( *NewSelection == TEXT("Single Convex Hull") )
	{
		NewBodyData->GeomType = EFG_SingleConvexHull;		
	}
	else if ( *NewSelection == TEXT("Multi Convex Hull") )
	{
		NewBodyData->GeomType = EFG_MultiConvexHull;		
	}
	else
	{
		NewBodyData->GeomType = EFG_Sphyl;
	}
}

void SPhATNewAssetDlg::OnWeightOptionSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (*NewSelection == TEXT("Dominant Weight"))
	{
		NewBodyData->VertWeight = EVW_DominantWeight;
	} 
	else
	{
		NewBodyData->VertWeight = EVW_AnyWeight;
	}
}

void SPhATNewAssetDlg::OnAngularConstraintModeSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (*NewSelection == TEXT("Limited"))
	{
		NewBodyData->AngularConstraintMode = ACM_Limited;
	} 
	else if (*NewSelection == TEXT("Locked"))
	{
		NewBodyData->AngularConstraintMode = ACM_Locked;
	}
	else if (*NewSelection == TEXT("Free"))
	{
		NewBodyData->AngularConstraintMode = ACM_Free;
	}
}

void SPhATNewAssetDlg::OnToggleOrientAlongBone(ECheckBoxState InCheckboxState)
{
	NewBodyData->bAlignDownBone = (InCheckboxState == ECheckBoxState::Checked);
}

void SPhATNewAssetDlg::OnToggleCreateJoints(ECheckBoxState InCheckboxState)
{
	NewBodyData->bCreateJoints = (InCheckboxState == ECheckBoxState::Checked);
}

void SPhATNewAssetDlg::OnToggleWalkPastSmallBones(ECheckBoxState InCheckboxState)
{
	NewBodyData->bWalkPastSmall = (InCheckboxState == ECheckBoxState::Checked);
}

void SPhATNewAssetDlg::OnToggleCreateBodyForAllBones(ECheckBoxState InCheckboxState)
{
	NewBodyData->bBodyForAll = (InCheckboxState == ECheckBoxState::Checked);
}

EVisibility SPhATNewAssetDlg::GetHullOptionsVisibility() const
{
	return NewBodyData->GeomType == EFG_MultiConvexHull ? EVisibility::Visible : EVisibility::Collapsed;
}

void SPhATNewAssetDlg::OnHullAccuracyChanged(float InNewValue)
{
	NewBodyData->HullAccuracy = InNewValue;
}

void SPhATNewAssetDlg::OnVertsPerHullCountChanged(int32 InNewValue)
{
	NewBodyData->MaxHullVerts = InNewValue;
}

float SPhATNewAssetDlg::GetHullAccuracy() const
{
	return NewBodyData->HullAccuracy;
}

int32 SPhATNewAssetDlg::GetVertsPerHullCount() const
{
	return NewBodyData->MaxHullVerts;
}
