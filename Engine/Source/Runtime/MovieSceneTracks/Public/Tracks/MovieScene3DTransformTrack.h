// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieScenePropertyTrack.h"
#include "MovieScene3DTransformTrack.generated.h"


/**
 * Handles manipulation of component transforms in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieScene3DTransformTrack : public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()
public:
	/** UMovieSceneTrack interface */
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;

	/**
	 * Evaluates the track at the playback position
	 *
	 * @param Position		The current playback position
	 * @param LastPosition		The last plackback position
	 * @param OutTranslation	The evalulated translation component of the transform
	 * @param OutRotation		The evalulated rotation component of the transform
	 * @param OutScale		The evalulated scale component of the transform
	 * @return true if anything was evaluated. Note: if false is returned OutBool remains unchanged
	 */
	virtual bool Eval( float Position, float LastPosition, FVector& OutTranslation, FRotator& OutRotation, FVector& OutScale ) const;
};
