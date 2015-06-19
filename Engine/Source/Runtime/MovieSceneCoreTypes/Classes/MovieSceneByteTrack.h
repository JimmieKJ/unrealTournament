// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieScenePropertyTrack.h"
#include "MovieSceneByteTrack.generated.h"

/**
 * Handles manipulation of float properties in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieSceneByteTrack : public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()

public:
	/** UMovieSceneTrack interface */
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;

	/**
	 * Adds a key to a section.  Will create the section if it doesn't exist
	 *
	 * @param Time				The time relative to the owning movie scene where the section should be
	 * @param Value				The value of the key
	 * @return True if the key was successfully added.
	 */
	virtual bool AddKeyToSection( float Time, uint8 Value );

	/**
	 * Evaluates the track at the playback position
	 *
	 * @param Position	The current playback position
	 * @param LastPosition	The last plackback position
	 * @param OutByte 	The value at the playback position
	 * @return true if anything was evaluated. Note: if false is returned OutByte remains unchanged
	 */
	virtual bool Eval( float Position, float LastPostion, uint8& OutByte ) const;
};
