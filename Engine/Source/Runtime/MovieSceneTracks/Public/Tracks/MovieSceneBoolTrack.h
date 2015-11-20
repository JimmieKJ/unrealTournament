// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieScenePropertyTrack.h"
#include "MovieSceneBoolTrack.generated.h"


/**
 * Handles manipulation of float properties in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieSceneBoolTrack
	: public UMovieScenePropertyTrack
{
	GENERATED_BODY()

public:

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;

	/**
	 * Evaluates the track at the playback position
	 *
	 * @param Position	The current playback position
	 * @param LastPosition	The last plackback position
	 * @param OutBool 	The value at the playback position
	 * @return true if anything was evaluated. Note: if false is returned OutBool remains unchanged
	 */
	virtual bool Eval( float Position, float LastPostion, bool& OutBool ) const;
};
