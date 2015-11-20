// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"


class UMovieScene3DTransformTrack;


/**
 * Instance of a UMovieSceneTransformTrack
 */
class FMovieScene3DTransformTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	FMovieScene3DTransformTrackInstance( UMovieScene3DTransformTrack& InTransformTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState (const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState (const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass ) override;
	virtual EMovieSceneUpdatePass HasUpdatePasses() override { return (EMovieSceneUpdatePass)(MSUP_PreUpdate | MSUP_Update); }
	virtual void RefreshInstance( const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}
	virtual void ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}

private:

	/** Track that is being instanced */
	UMovieScene3DTransformTrack* TransformTrack;

	/** Map from object to initial state */
	TMap< TWeakObjectPtr<UObject>, FTransform > InitTransformMap;
};
