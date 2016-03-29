// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A widget for editing a curve representing integer keys.
 */
class SIntegralCurveKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SIntegralCurveKeyEditor) {}
		SLATE_ARGUMENT(ISequencer*, Sequencer)
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)
		SLATE_ARGUMENT(FIntegralCurve*, Curve)
		SLATE_ATTRIBUTE(TOptional<uint8>, IntermediateValue)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

private:
	void OnBeginSliderMovement();

	void OnEndSliderMovement(int32 Value);

	int32 OnGetKeyValue() const;

	void OnValueChanged(int32 Value);
	void OnValueCommitted(int32 Value, ETextCommit::Type CommitInfo);

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FIntegralCurve* Curve;
	TAttribute<TOptional<uint8>> IntermediateValue;
};