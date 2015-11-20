// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"

/**
 * Instance of a UMovieScene2DTransformTrack
 */
class FMovieScene2DTransformTrackInstance : public IMovieSceneTrackInstance
{
public:
	FMovieScene2DTransformTrackInstance( class UMovieScene2DTransformTrack& InTransformTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void RestoreState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass ) override;
	virtual void RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override;
	virtual void ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {};
private:
	/** Track that is being instanced */
	UMovieScene2DTransformTrack* TransformTrack;
	/** Runtime property bindings */
	TSharedPtr<class FTrackInstancePropertyBindings> PropertyBindings;
};
