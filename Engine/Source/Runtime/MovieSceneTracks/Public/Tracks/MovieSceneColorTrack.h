// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieScenePropertyTrack.h"
#include "MovieSceneColorTrack.generated.h"

/**
 * Handles manipulation of float properties in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieSceneColorTrack : public UMovieScenePropertyTrack
{
	GENERATED_UCLASS_BODY()

public:
	/** UMovieSceneTrack interface */
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;

	/**
	 * Evaluates the track at the playback position
	 *
	 * @param Position	The current playback position
	 * @param LastPosition	The last plackback position
	 * @param The color at the playback position
	 * @return true if anything was evaluated. Note: if false is returned OutColor remains unchanged
	 */
	virtual bool Eval( float Position, float LastPostion, FLinearColor& OutColor ) const;

private:
	UPROPERTY()
	bool bIsSlateColor_DEPRECATED;
};
