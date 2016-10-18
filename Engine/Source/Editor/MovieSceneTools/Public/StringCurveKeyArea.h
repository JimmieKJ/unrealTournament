// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NamedKeyArea.h"
#include "ClipboardTypes.h"


struct FStringCurve;
class UMovieSceneSection;


/**
 * A key area for string keys.
 */
class MOVIESCENETOOLS_API FStringCurveKeyArea
	: public FNamedKeyArea
{
public:

	FStringCurveKeyArea(FStringCurve* InCurve, UMovieSceneSection* InOwningSection, TOptional<FLinearColor> InColor = TOptional<FLinearColor>())
		: Color(InColor)
		, Curve(InCurve)
		, OwningSection(InOwningSection)
	{ }

public:

	void ClearIntermediateValue()
	{
		IntermediateValue.Reset();
	}

	void SetIntermediateValue(FString InIntermediateValue)
	{
		IntermediateValue = InIntermediateValue;
	}

public:

	//` IKeyArea interface

	virtual TArray<FKeyHandle> AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom = FLT_MAX) override;
	virtual TOptional<FKeyHandle> DuplicateKey(FKeyHandle KeyToDuplicate) override;
	virtual bool CanCreateKeyEditor() override;
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;
	virtual void DeleteKey(FKeyHandle KeyHandle) override;
	virtual TOptional<FLinearColor> GetColor() override;
	virtual ERichCurveExtrapolation GetExtrapolationMode(bool bPreInfinity) const override;
	virtual ERichCurveInterpMode GetKeyInterpMode(FKeyHandle KeyHandle) const override;
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(FKeyHandle KeyHandle) override;
	virtual ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const override;
	virtual float GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual UMovieSceneSection* GetOwningSection() override;
	virtual FRichCurve* GetRichCurve() override;
	virtual TArray<FKeyHandle> GetUnsortedKeyHandles() const override;
	virtual FKeyHandle MoveKey(FKeyHandle KeyHandle, float DeltaPosition) override;
	virtual void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) override;
	virtual bool CanSetExtrapolationMode() const override;
	virtual void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode) override;
	virtual void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode) override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime) const override;
	virtual void CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const override;
	virtual void PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment) override;

protected:

	void OnValueChanged(FString InValue);

private:

	/** The key area's color. */
	TOptional<FLinearColor> Color;

	/** The curve which provides the keys for this key area. */
	FStringCurve* Curve;

	/** The section that owns this key area. */
	UMovieSceneSection* OwningSection;

	TOptional<FString> IntermediateValue;
};
