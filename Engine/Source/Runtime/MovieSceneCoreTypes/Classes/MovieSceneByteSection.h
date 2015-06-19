// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneByteSection.generated.h"

/**
 * A single byte section
 */
UCLASS( MinimalAPI )
class UMovieSceneByteSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:
	/**
	 * Updates this section
	 *
	 * @param Position	The position in time within the movie scene
	 */
	virtual uint8 Eval( float Position ) const;

	/** 
	 * Adds a key to the section
	 *
	 * @param Time	The location in time where the key should be added
	 * @param Value	The value of the key
	 */
	void AddKey( float Time, uint8 Value );
	
	/** 
	 * Determines if a new key would be new data, or just a duplicate of existing data
	 *
	 * @param Time	The location in time where the key would be added
	 * @param Value	The value of the new key
	 * @return True if the new key would be new data, false if duplicate
	 */
	bool NewKeyIsNewData(float Time, uint8 Value) const;

	/**
	 * UMovieSceneSection interface 
	 */
	virtual void MoveSection( float DeltaPosition ) override;
	virtual void DilateSection( float DilationFactor, float Origin ) override;

	/** Gets all the keys of this byte section */
	FIntegralCurve& GetCurve() { return ByteCurve; }

private:
	/** Ordered curve data */
	// @todo Sequencer This could be optimized by packing the bytes separately
	// but that may not be worth the effort
	UPROPERTY()
	FIntegralCurve ByteCurve;
};
