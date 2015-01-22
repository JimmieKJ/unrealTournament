// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SubMovieSceneSection.generated.h"

/**
 * A container for a movie scene within a movie scene
 */
UCLASS( MinimalAPI )
class USubMovieSceneSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:
	/**
	 * Sets the movie scene played by this section
	 *
	 * @param InMovieScene	The movie scene to play
	 */
	void SetMovieScene( UMovieScene* InMovieScene ) { MovieScene = InMovieScene; }

	/** @return The movie scene played by this section */
	UMovieScene* GetMovieScene() const { return MovieScene; }
private:
	/** Movie scene being played by this section */
	UPROPERTY()
	UMovieScene* MovieScene;

	/** The start time where the movie scene should begin playing */
	UPROPERTY()
	float MovieSceneStartTime;

	/** The end time where the movie scene should begin playing */
	UPROPERTY()
	float MovieSceneTimeEndTime;
};
