// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSequenceInstance.h"

#include "MovieSceneSequence.h"
#include "MovieSceneSpawnTrack.h"
#include "MovieSceneSpawnTrackInstance.h"


FMovieSceneSpawnTrackInstance::FMovieSceneSpawnTrackInstance(UMovieSceneSpawnTrack& InTrack)
{
	Track = &InTrack;
}

void FMovieSceneSpawnTrackInstance::Update(float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass)
{
	IMovieSceneSpawnRegister& SpawnRegister = Player.GetSpawnRegister();
	FMovieSceneSpawnable* Spawnable = SequenceInstance.GetSequence()->GetMovieScene()->FindSpawnable(Track->GetObjectId());

	TRange<float> Range = SequenceInstance.GetTimeRange();

	// If we're evaluating outside of the instance's time range, and the sequence owns the spawnable, there's no reason to evaluate - it should already be destroyed
	if (Spawnable && Spawnable->GetSpawnOwnership() == ESpawnOwnership::InnerSequence && !Range.Contains(Position) && !Range.Contains(LastPosition))
	{
		SpawnRegister.DestroySpawnedObject(Track->GetObjectId(), SequenceInstance, Player);
		return;
	}

	bool bIsSpawned = false;
	if (Track->Eval(Position, LastPosition, bIsSpawned))
	{
		// Spawn the object if needed
		if (bIsSpawned && RuntimeObjects.Num() == 0)
		{
			SpawnRegister.SpawnObject(Track->GetObjectId(), SequenceInstance, Player);
		}

		// Destroy the object if needed
		if (!bIsSpawned && RuntimeObjects.Num() != 0)
		{
			SpawnRegister.DestroySpawnedObject(Track->GetObjectId(), SequenceInstance, Player);
		}
	}
}