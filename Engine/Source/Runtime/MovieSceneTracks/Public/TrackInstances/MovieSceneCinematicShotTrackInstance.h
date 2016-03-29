// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSubTrackInstance.h"


class FMovieSceneSequenceInstance;
class UMovieSceneCinematicShotSection;
class UMovieSceneCinematicShotTrack;


/**
 * Instance of a UMovieSceneSubTrack.
 */
class FMovieSceneCinematicShotTrackInstance
	: public FMovieSceneSubTrackInstance
{
public:

	/** Create and initialize a new instance. */
	FMovieSceneCinematicShotTrackInstance(UMovieSceneCinematicShotTrack& InTrack);

	// IMovieSceneTrackInstance interface

	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState (const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { }
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;

protected:

	// FMovieSceneSubTrackinstance
	virtual bool ShouldEvaluateIfOverlapping(const TArray<UMovieSceneSection*>& Sections, UMovieSceneSection* Section) const override;
};
