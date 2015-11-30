// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneTrackInstance.h"

class UMovieSceneSpawnTrack;

/**
 * Instance of a spawn track that actually spawns/destroys objects based on track evaluation
 */
class FMovieSceneSpawnTrackInstance : public IMovieSceneTrackInstance
{
public:
	FMovieSceneSpawnTrackInstance(UMovieSceneSpawnTrack& InTrack);

	/** IMovieSceneTrackInstance interface */
	virtual void SaveState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void RestoreState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void RefreshInstance(const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual void ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) override {}
	virtual EMovieSceneUpdatePass HasUpdatePasses() override { return MSUP_PreUpdate; }

	virtual void Update(float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass) override;

	virtual float EvalOrder() override { return -10000.f; }

private:
	/** Spawn track that is being instanced */
	UMovieSceneSpawnTrack* Track;
};
