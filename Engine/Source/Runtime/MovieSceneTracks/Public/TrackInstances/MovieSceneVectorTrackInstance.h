// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"


class FTrackInstancePropertyBindings;
class UMovieSceneVectorTrack;


/**
 * Instance of a UMovieSceneVectorTrack
 */
class FMovieSceneVectorTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	FMovieSceneVectorTrackInstance( UMovieSceneVectorTrack& InVectorTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}

private:

	/** Track that is being instanced */
	UMovieSceneVectorTrack* VectorTrack;

	/** Runtime property bindings */
	TSharedPtr<FTrackInstancePropertyBindings> PropertyBindings;

	/** Map from object to initial state */
	TMap< TWeakObjectPtr<UObject>, FVector2D > InitVector2DMap;
	TMap< TWeakObjectPtr<UObject>, FVector > InitVector3DMap;
	TMap< TWeakObjectPtr<UObject>, FVector4 > InitVector4DMap;
};
