// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FPhATSharedData;

/*-----------------------------------------------------------------------------
   SPhATViewport
-----------------------------------------------------------------------------*/

class SPhATNewAssetDlg : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPhATNewAssetDlg)
		{}

		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
			SLATE_ATTRIBUTE(FPhysAssetCreateParams*, NewBodyData)
			SLATE_ATTRIBUTE(EAppReturnType::Type*, NewBodyResponse)
	SLATE_END_ARGS()

	/** SCompoundWidget interface */
	void Construct(const FArguments& InArgs);

	/** SWidget interface */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override;

private:
	/** Makes option widgets for combobox controls */
	TSharedRef<SWidget> MakeTextWidgetOption(TSharedPtr<FString> InItem);

	/** Handles Ok/Cancel button clicks */
	FReply OnClicked(EAppReturnType::Type InResponse);

	/** Parameter changed handlers */
	void OnMinimumBoneSizeCommitted(float NewValue, ETextCommit::Type CommitInfo);
	void OnCollisionGeometrySelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnWeightOptionSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnAngularConstraintModeSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnToggleOrientAlongBone(ECheckBoxState InCheckboxState);
	void OnToggleCreateJoints(ECheckBoxState InCheckboxState);
	void OnToggleWalkPastSmallBones(ECheckBoxState InCheckboxState);
	void OnToggleCreateBodyForAllBones(ECheckBoxState InCheckboxState);
	
	EVisibility GetHullOptionsVisibility() const;
	void OnHullAccuracyChanged(float InNewValue);	
	void OnVertsPerHullCountChanged(int32 InNewValue);
	float GetHullAccuracy() const;
	int32 GetVertsPerHullCount() const;

	/** returns MinBoneSize for widget **/
	TOptional<float> GetMinBoneSize() const;

private:
	/** Parent window */
	TWeakPtr<SWindow> ParentWindow;

	/** Results from the new body dialog */
	FPhysAssetCreateParams* NewBodyData;
	EAppReturnType::Type* NewBodyResponse;

	/** Combobox options */
	TArray< TSharedPtr<FString> > CollisionGeometryOptions;
	TArray< TSharedPtr<FString> > WeightOptions;
	TArray< TSharedPtr<FString> > AngularConstraintModes;

	TSharedPtr< SSpinBox<float> > HullAccuracy;
	TSharedPtr< SSpinBox<int32> > MaxVertsPerHull;
};
