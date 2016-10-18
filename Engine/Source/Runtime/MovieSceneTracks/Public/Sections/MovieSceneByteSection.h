// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/IntegralCurve.h"
#include "MovieSceneSection.h"
#include "IKeyframeSection.h"
#include "MovieSceneByteSection.generated.h"


/**
 * A single byte section.
 */
UCLASS(MinimalAPI)
class UMovieSceneByteSection 
	: public UMovieSceneSection
	, public IKeyframeSection<uint8>
{
	GENERATED_UCLASS_BODY()
public:
	/**
	 * Update this section.
	 *
	 * @param Position The position in time within the movie scene
	 */
	virtual uint8 Eval(float Position) const;

	/** Gets all the keys of this byte section */
	FIntegralCurve& GetCurve()
	{
		return ByteCurve;
	}

public:

	//~ IKeyframeSection interface

	virtual void AddKey(float Time, const uint8& Value, EMovieSceneKeyInterpolation KeyInterpolation) override;
	virtual bool NewKeyIsNewData(float Time, const uint8& Value) const override;
	virtual bool HasKeys(const uint8& Value) const override;
	virtual void SetDefault(const uint8& Value) override;

public:

	//~ UMovieSceneSection interface 

	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TOptional<float> GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float Time) override;

private:

	/** Ordered curve data */
	// @todo Sequencer This could be optimized by packing the bytes separately
	// but that may not be worth the effort
	UPROPERTY()
	FIntegralCurve ByteCurve;
};
