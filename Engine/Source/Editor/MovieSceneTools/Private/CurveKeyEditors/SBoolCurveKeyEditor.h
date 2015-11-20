// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A widget for editing a curve representing bool keys.
 */
class SBoolCurveKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBoolCurveKeyEditor) {}
		SLATE_ARGUMENT(ISequencer*, Sequencer)
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)
		SLATE_ARGUMENT(FIntegralCurve*, Curve)
		SLATE_ATTRIBUTE(TOptional<bool>, IntermediateValue)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

private:

	ECheckBoxState IsChecked() const;
	void OnCheckStateChanged(ECheckBoxState);

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FIntegralCurve* Curve;
	TAttribute<TOptional<bool>> IntermediateValue;
};