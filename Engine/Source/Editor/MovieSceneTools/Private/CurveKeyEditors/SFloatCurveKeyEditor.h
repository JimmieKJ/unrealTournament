// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A widget for editing a curve representing float keys.
 */
class SFloatCurveKeyEditor : public SCompoundWidget
{
public:
	/** Notification for numeric value change */
	DECLARE_DELEGATE_OneParam( FOnValueChanged, float );

	SLATE_BEGIN_ARGS(SFloatCurveKeyEditor) {}
		SLATE_ARGUMENT(ISequencer*, Sequencer)
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)
		SLATE_ARGUMENT(FRichCurve*, Curve)
		SLATE_EVENT(FOnValueChanged, OnValueChanged)
		SLATE_ATTRIBUTE(TOptional<float>, IntermediateValue)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

private:
	void OnBeginSliderMovement();

	void OnEndSliderMovement(float Value);

	float OnGetKeyValue() const;

	void OnValueChanged(float Value);
	void OnValueCommitted(float Value, ETextCommit::Type CommitInfo);

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FRichCurve* Curve;
	FOnValueChanged OnValueChangedEvent;
	TAttribute<TOptional<float>> IntermediateValue;
};