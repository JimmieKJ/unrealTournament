// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A widget for editing a curve representing enum keys.
 */
class SEnumCurveKeyEditor : public SCompoundWidget
{
public:
	/** Notification for enum value change */
	DECLARE_DELEGATE_OneParam( FOnValueChanged, int32 );

	SLATE_BEGIN_ARGS(SEnumCurveKeyEditor) {}
		SLATE_ARGUMENT(ISequencer*, Sequencer)
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)
		SLATE_ARGUMENT(FIntegralCurve*, Curve)
		SLATE_EVENT(FOnValueChanged, OnValueChanged)
		SLATE_ARGUMENT(const UEnum*, Enum)
		SLATE_ATTRIBUTE(TOptional<uint8>, IntermediateValue)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

private:
	int32 OnGetCurrentValue() const;

	void OnComboSelectionChanged(int32 InSelectedItem, ESelectInfo::Type SelectInfo);

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FIntegralCurve* Curve;
	FOnValueChanged OnValueChangedEvent;
};