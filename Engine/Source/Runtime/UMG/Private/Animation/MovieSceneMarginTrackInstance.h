// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	virtual void SaveState( const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}
	virtual void RestoreState( const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}
	virtual void Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass ) override;
	virtual void RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override;
	virtual void ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {};
private:
	/** The track being instanced */
	UMovieSceneMarginTrack* MarginTrack;
	/** Runtime property bindings */
	TSharedPtr<class FTrackInstancePropertyBindings> PropertyBindings;
};
