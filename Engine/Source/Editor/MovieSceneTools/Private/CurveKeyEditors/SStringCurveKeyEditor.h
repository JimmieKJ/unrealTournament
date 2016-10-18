// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A widget for editing a curve representing string keys.
 */
class SStringCurveKeyEditor : public SCompoundWidget
{
public:
	/** Notification for string value change */
	DECLARE_DELEGATE_OneParam( FOnValueChanged, FString );

	SLATE_BEGIN_ARGS(SStringCurveKeyEditor) {}
		SLATE_ARGUMENT(ISequencer*, Sequencer)
		SLATE_ARGUMENT(UMovieSceneSection*, OwningSection)
		SLATE_ARGUMENT(FStringCurve*, Curve)
		SLATE_EVENT(FOnValueChanged, OnValueChanged)
		SLATE_ATTRIBUTE(TOptional<FString>, IntermediateValue)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

private:

	FText GetText() const;
	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType);

	ISequencer* Sequencer;
	UMovieSceneSection* OwningSection;
	FStringCurve* Curve;
	FOnValueChanged OnValueChangedEvent;
	TAttribute<TOptional<FString>> IntermediateValue;
};