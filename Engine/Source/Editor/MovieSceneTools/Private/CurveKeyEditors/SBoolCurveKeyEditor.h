// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A widget for editing a curve representing bool keys.
 */
class SBoolCurveKeyEditor : public SCompoundWidget
{
public:
	/** Notification for bool value change */
	DECLARE_DELEGATE_OneParam( FOnValueChanged, bool );

	SLATE_BEGIN_ARGS(SBoolCurveKeyEditor) {}
		SLATE_ARGUMENT(ISequencer*, Sequencer)
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)
		SLATE_ARGUMENT(FIntegralCurve*, Curve)
		SLATE_EVENT(FOnValueChanged, OnValueChanged)
		SLATE_ATTRIBUTE(TOptional<bool>, IntermediateValue)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

private:

	ECheckBoxState IsChecked() const;
	void OnCheckStateChanged(ECheckBoxState);

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FIntegralCurve* Curve;
	FOnValueChanged OnValueChangedEvent;
	TAttribute<TOptional<bool>> IntermediateValue;
};