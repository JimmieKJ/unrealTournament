// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "IKeyframeSection.h"
#include "MovieSceneFloatSection.generated.h"

/**
 * A single floating point section
 */
UCLASS( MinimalAPI )
class UMovieSceneFloatSection
	: public UMovieSceneSection
	, public IKeyframeSection<float>
{
	GENERATED_UCLASS_BODY()
public:
	/**
	 * Updates this section
	 *
	 * @param Position	The position in time within the movie scene
	 */
	float Eval( float Position ) const;

	// IKeyframeSection interface.

	void AddKey( float Time, const float& Value, EMovieSceneKeyInterpolation KeyInterpolation );
	bool NewKeyIsNewData(float Time, const float& Value) const;
	bool HasKeys( const float& Value ) const;
	void SetDefault( const float& Value );

	/**
	 * UMovieSceneSection interface 
	 */
	virtual void MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles) override;
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const override;

	/**
	 * @return The float curve on this section
	 */
	FRichCurve& GetFloatCurve() { return FloatCurve; }
private:
	/** Curve data */
	UPROPERTY()
	FRichCurve FloatCurve;
};