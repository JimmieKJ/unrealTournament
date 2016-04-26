// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"


class UMovieSceneFadeTrack;


/**
 * Instance of a UMovieSceneFadeTrack
 */
class FMovieSceneFadeTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InFadeTrack The event track to create an instance for.
	 */
	FMovieSceneFadeTrackInstance(UMovieSceneFadeTrack& InFadeTrack);

	/** Virtual destructor. */
	virtual ~FMovieSceneFadeTrackInstance() { }

public:

	// IMovieSceneTrackInstance interface

	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { }
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { }
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override { }
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;

private:

	/** Track that is being instanced */
	UMovieSceneFadeTrack* FadeTrack;

	/** A map from the viewport client to the viewport params that is stored before the track takes control and restored when the track relinquishes control */
	TMap<TSharedPtr<FViewportClient>, EMovieSceneViewportParams> ViewportParamMap;
};
