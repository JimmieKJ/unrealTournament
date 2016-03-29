// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScenePropertyTrack.h"
#include "KeyParams.h"
#include "MovieSceneMarginTrack.generated.h"


/**
 * Handles manipulation of FMargins in a movie scene
 */
UCLASS( MinimalAPI )
class UMovieSceneMarginTrack : public UMovieScenePropertyTrack
{
	GENERATED_BODY()

public:

	// UMovieSceneTrack interface

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual TSharedPtr<IMovieSceneTrackInstance> CreateInstance() override;

	/**
	 * Evaluates the track at the playback position
	 *
	 * @param Position The current playback position
	 * @param LastPosition The last playback position
	 * @param InOutMargin The margin at the playback position
	 * @return true if anything was evaluated. Note: if false is returned OutMargin remains unchanged
	 */
	bool Eval( float Position, float LastPostion, FMargin& InOutMargin ) const;
};
