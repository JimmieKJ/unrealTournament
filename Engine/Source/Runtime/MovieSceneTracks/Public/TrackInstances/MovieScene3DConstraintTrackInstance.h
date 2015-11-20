// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"

class UMovieScene3DConstraintTrack;
class UMovieScene3DConstraintSection;

/**
 * Instance of a UMovieScene3DConstraintTrack
 */
class FMovieScene3DConstraintTrackInstance
	: public IMovieSceneTrackInstance
{
public:
	FMovieScene3DConstraintTrackInstance( UMovieScene3DConstraintTrack& InConstraintTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState (const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState (const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass ) override;
	virtual void RefreshInstance( const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}
	virtual void ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}
	virtual float EvalOrder() { return -1.f; }

	virtual void UpdateConstraint(float Position, const TArray<UObject*>& RuntimeObjects, AActor* Actor, UMovieScene3DConstraintSection* ConstraintSection) = 0;

protected:
	/** Track that is being instanced */
	UMovieScene3DConstraintTrack* ConstraintTrack;

	/** Map from object to initial state */
	TMap< TWeakObjectPtr<UObject>, FTransform > InitTransformMap;
};
