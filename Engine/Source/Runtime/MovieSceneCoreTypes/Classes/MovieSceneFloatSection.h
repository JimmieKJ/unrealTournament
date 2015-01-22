// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneFloatSection.generated.h"

/**
 * A single floating point section
 */
UCLASS( MinimalAPI )
class UMovieSceneFloatSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:
	/**
	 * Updates this section
	 *
	 * @param Position	The position in time within the movie scene
	 */
	float Eval( float Position ) const;

	/** 
	 * Adds a key to the section
	 *
	 * @param Time	The location in time where the key should be added
	 * @param Value	The value of the key
	 */
	void AddKey( float Time, float Value );
	
	/** 
	 * Determines if a new key would be new data, or just a duplicate of existing data
	 *
	 * @param Time	The location in time where the key would be added
	 * @param Value	The value of the new key
	 * @return True if the new key would be new data, false if duplicate
	 */
	bool NewKeyIsNewData(float Time, float Value) const;

	/**
	 * UMovieSceneSection interface 
	 */
	virtual void MoveSection( float DeltaPosition ) override;
	virtual void DilateSection( float DilationFactor, float Origin ) override;

	/**
	 * @return The float curve on this section
	 */
	FRichCurve& GetFloatCurve() { return FloatCurve; }
private:
	/** Curve data */
	UPROPERTY()
	FRichCurve FloatCurve;
};