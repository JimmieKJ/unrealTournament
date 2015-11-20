// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "IKeyframeSection.h"
#include "MovieScene2DTransformSection.generated.h"


enum class EKey2DTransformChannel
{
	Translation,
	Rotation,
	Scale,
	Shear
};


enum class EKey2DTransformAxis
{
	X,
	Y,
	None
};


struct F2DTransformKey
{
	F2DTransformKey( EKey2DTransformChannel InChannel, EKey2DTransformAxis InAxis, float InValue )
	{
		Channel = InChannel;
		Axis = InAxis;
		Value = InValue;
	}
	EKey2DTransformChannel Channel;
	EKey2DTransformAxis Axis;
	float Value;
};


/**
 * A transform section
 */
UCLASS(MinimalAPI)
class UMovieScene2DTransformSection
	: public UMovieSceneSection
	, public IKeyframeSection<F2DTransformKey>
{
	GENERATED_BODY()

public:

	// UMovieSceneSection interface

	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const override;

public:

	UMG_API FRichCurve& GetTranslationCurve( EAxis::Type Axis );

	UMG_API FRichCurve& GetRotationCurve();

	UMG_API FRichCurve& GetScaleCurve( EAxis::Type Axis );

	UMG_API FRichCurve& GetSheerCurve( EAxis::Type Axis );

	FWidgetTransform Eval( float Position, const FWidgetTransform& DefaultValue ) const;

	// IKeyframeSection interface.
	virtual bool NewKeyIsNewData( float Time, const struct F2DTransformKey& TransformKey ) const override;
	virtual bool HasKeys( const struct F2DTransformKey& TransformKey ) const override;
	virtual void AddKey( float Time, const struct F2DTransformKey& TransformKey, EMovieSceneKeyInterpolation KeyInterpolation ) override;
	virtual void SetDefault( const struct F2DTransformKey& TransformKey ) override;

private:

	/** Translation curves*/
	UPROPERTY()
	FRichCurve Translation[2];
	
	/** Rotation curve */
	UPROPERTY()
	FRichCurve Rotation;

	/** Scale curves */
	UPROPERTY()
	FRichCurve Scale[2];

	/** Shear curve */
	UPROPERTY()
	FRichCurve Shear[2];
};
