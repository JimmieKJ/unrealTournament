// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieScenePropertyTrack.h"
#include "MovieScene3DTransformTrack.generated.h"


/**
 * Handles manipulation of component transforms in a movie scene
 */
UCLASS(MinimalAPI)
class UMovieScene3DTransformTrack
	: public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Evaluates the track at the playback position
	 *
	 * @param Position The current playback position.
	 * @param LastPosition The last playback position.
	 * @param OutTranslation The evaluated translation component of the transform.
	 * @param OutRotation The evaluated rotation component of the transform.
	 * @param OutScale The evaluated scale component of the transform.
	 * @return true if anything was evaluated. Note: if false is returned OutBool remains unchanged.
	 */
	virtual bool Eval(float Position, float LastPosition, FVector& OutTranslation, FRotator& OutRotation, FVector& OutScale) const;

public:

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;
};
