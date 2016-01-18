// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A widget for editing a curve representing enum keys.
 */
class SEnumCurveKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEnumCurveKeyEditor) {}
		SLATE_ARGUMENT(ISequencer*, Sequencer)
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)
		SLATE_ARGUMENT(FIntegralCurve*, Curve)
		SLATE_ARGUMENT(const UEnum*, Enum)
		SLATE_ATTRIBUTE( TOptional<uint8>, IntermediateValue )
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

private:
	FText GetCurrentValue() const;

	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<int32> InItem);

	void OnComboSelectionChanged(TSharedPtr<int32> InSelectedItem, ESelectInfo::Type SelectInfo);

	void OnComboMenuOpening();

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FIntegralCurve* Curve;
	const UEnum* Enum;
	TArray<TSharedPtr<int32>> VisibleEnumNameIndices;
	TSharedPtr<SComboBox<TSharedPtr<int32>>> EnumValuesCombo;
	bool bUpdatingSelectionInternally;
	TAttribute<TOptional<uint8>> IntermediateValue;
};