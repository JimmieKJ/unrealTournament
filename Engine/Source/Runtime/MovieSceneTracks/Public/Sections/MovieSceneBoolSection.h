// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneBoolSection.generated.h"

/**
 * A single bool section
 */
UCLASS( MinimalAPI )
class UMovieSceneBoolSection 
	: public UMovieSceneSection
	, public IKeyframeSection<bool>
{
	GENERATED_UCLASS_BODY()

public:

	/** The default value to use when no keys are present */
	UPROPERTY(EditAnywhere, Category="Curve")
	bool DefaultValue;

public:

	/**
	 * Updates this section
	 *
	 * @param Position	The position in time within the movie scene
	 */
	virtual bool Eval( float Position ) const;

	// IKeyframeSection interface.
	virtual void AddKey( float Time, const bool& Value, EMovieSceneKeyInterpolation KeyInterpolation ) override;
	virtual void SetDefault( const bool& Value ) override;
	virtual bool NewKeyIsNewData(float Time, const bool& Value ) const override;
	virtual bool HasKeys( const bool& Value ) const;

	/**
	 * UMovieSceneSection interface 
	 */
	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const override;

	/** Gets all the keys of this boolean section */
	FIntegralCurve& GetCurve() { return BoolCurve; }

private:
	/** Ordered curve data */
	// @todo Sequencer This could be optimized by packing the bools separately
	// but that may not be worth the effort
	UPROPERTY(EditAnywhere, Category="Curve")
	FIntegralCurve BoolCurve;
};
