// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"


class UMovieSceneEventTrack;


/**
 * Instance of a UMovieSceneEventTrack
 */
class FMovieSceneEventTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InEventTrack The event track to create an instance for.
	 */
	FMovieSceneEventTrackInstance(UMovieSceneEventTrack& InEventTrack );

	/** Virtual destructor. */
	virtual ~FMovieSceneEventTrackInstance() { }

public:

	// IMovieSceneTrackInstance interface

	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { }
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { }
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { }
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { }
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;

private:

	/** Track that is being instanced */
	UMovieSceneEventTrack* EventTrack;
};
