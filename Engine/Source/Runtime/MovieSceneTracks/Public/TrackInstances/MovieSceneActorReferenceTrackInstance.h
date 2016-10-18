// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectKey.h"
#include "IMovieSceneTrackInstance.h"


class FTrackInstancePropertyBindings;
class UMovieSceneActorReferenceTrack;


/**
 * Runtime instance of a UMovieSceneActorReferenceTrack
 */
class FMovieSceneActorReferenceTrackInstance
	: public IMovieSceneTrackInstance
{
public:

	FMovieSceneActorReferenceTrackInstance( UMovieSceneActorReferenceTrack& InActorReferenceTrack );

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState (const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RestoreState (const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override;
	virtual void RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override;
	virtual void ClearInstance( IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) override {}

private:

	/** Track that is being instanced */
	UMovieSceneActorReferenceTrack* ActorReferenceTrack;

	/** Runtime property bindings */
	TSharedPtr<FTrackInstancePropertyBindings> PropertyBindings;

	/** Map from object to initial state */
	TMap<FObjectKey, TWeakObjectPtr<AActor>> InitActorReferenceMap;

	/** Map from guid to cached actor */
	TMap<FGuid, TWeakObjectPtr<AActor>> GuidToActorCache;
};
