// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"


class FTrackInstancePropertyBindings;
class UMovieSceneFloatTrack;


/**
 * Instance of a UMovieSceneFloatTrack
 */
class FMovieSceneFloatTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	FMovieSceneFloatTrackInstance( UMovieSceneFloatTrack& InFloatTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override;
	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}

private:

	/** Track that is being instanced */
	UMovieSceneFloatTrack* FloatTrack;

	/** Runtime property bindings */
	TSharedPtr<FTrackInstancePropertyBindings> PropertyBindings;

	/** Map from object to initial state */
	TMap<TWeakObjectPtr<UObject>, float> InitFloatMap;
};
