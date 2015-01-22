// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"

#include "MovieSceneMarginSection.generated.h"

struct FMarginKey;

/**
 * A section in a Margin track
 */
UCLASS(MinimalAPI)
class UMovieSceneMarginSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:
	/** UMovieSceneSection interface */
	virtual void MoveSection( float DeltaPosition ) override;
	virtual void DilateSection( float DilationFactor, float Origin ) override;

	/**
	 * Updates this section
	 *
	 * @param Position	The position in time within the movie scene
	 */
	FMargin Eval( float Position, const FMargin& DefaultValue ) const;

	/** 
	 * Adds a key to the section
	 *
	 * @param Time	The location in time where the key should be added
	 * @param Value	The value of the key
	 */
	void AddKey( float Time, const FMarginKey& MarginKey );
	
	/** 
	 * Determines if a new key would be new data, or just a duplicate of existing data
	 *
	 * @param Time	The location in time where the key would be added
	 * @param Value	The value of the new key
	 * @return True if the new key would be new data, false if duplicate
	 */
	bool NewKeyIsNewData(float Time, const FMargin& Value) const;

	/**
	 * Gets the top curve
	 *
	 * @return The rich curve for this margin channel
	 */
	FRichCurve& GetTopCurve() { return TopCurve; }
	const FRichCurve& GetTopCurve() const { return TopCurve; }

	/**
	 * Gets the left curve
	 *
	 * @return The rich curve for this margin channel
	 */
	FRichCurve& GetLeftCurve() { return LeftCurve; }
	const FRichCurve& GetLeftCurve() const { return LeftCurve; }
	/**
	 * Gets the right curve
	 *
	 * @return The rich curve for this margin channel
	 */
	FRichCurve& GetRightCurve() { return RightCurve; }
	const FRichCurve& GetRightCurve() const { return RightCurve; }
	
	/**
	 * Gets the bottom curve
	 *
	 * @return The rich curve for this margin channel
	 */
	FRichCurve& GetBottomCurve() { return BottomCurve; }
	const FRichCurve& GetBottomCurve() const { return BottomCurve; }
private:
	void AddKeyToNamedCurve( float Time, const FMarginKey& MarginKey );

private:
	/** Red curve data */
	UPROPERTY()
	FRichCurve TopCurve;

	/** Green curve data */
	UPROPERTY()
	FRichCurve LeftCurve;

	/** Blue curve data */
	UPROPERTY()
	FRichCurve RightCurve;

	/** Alpha curve data */
	UPROPERTY()
	FRichCurve BottomCurve;
};
