// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	virtual void SaveState (const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState (const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}
	virtual void ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}

	// Evaluate after TransformTrackInstance so that parents can be evaluated first
	virtual EMovieSceneUpdatePass HasUpdatePasses() override { return MSUP_PostUpdate; }

	virtual void UpdateConstraint(float Position, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, AActor* Actor, UMovieScene3DConstraintSection* ConstraintSection) = 0;

protected:
	/** Track that is being instanced */
	UMovieScene3DConstraintTrack* ConstraintTrack;

	/** Map from object to initial state */
	TMap< TWeakObjectPtr<UObject>, FTransform > InitTransformMap;
};
