// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"

/**
 * Instance of a UMovieSceneMarginTrack
 */
class FMovieSceneMarginTrackInstance : public IMovieSceneTrackInstance
{
public:
	FMovieSceneMarginTrackInstance( class UMovieSceneMarginTrack& InMarginTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}
	virtual void RestoreState( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override;
	virtual void ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {};
private:
	/** The track being instanced */
	UMovieSceneMarginTrack* MarginTrack;
	/** Runtime property bindings */
	TSharedPtr<class FTrackInstancePropertyBindings> PropertyBindings;
};
