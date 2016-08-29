// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSequenceInstance.h"

#include "MovieSceneSequence.h"
#include "MovieSceneSpawnTrack.h"
#include "MovieSceneSpawnTrackInstance.h"


FMovieSceneSpawnTrackInstance::FMovieSceneSpawnTrackInstance(UMovieSceneSpawnTrack& InTrack)
{
	Track = &InTrack;
}

void FMovieSceneSpawnTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	IMovieSceneSpawnRegister& SpawnRegister = Player.GetSpawnRegister();
	FMovieSceneSpawnable* Spawnable = SequenceInstance.GetSequence()->GetMovieScene()->FindSpawnable(Track->GetObjectId());

	TRange<float> Range = SequenceInstance.GetTimeRange();

	const bool bIsPreview = Player.IsPreview();

	// If we're evaluating outside of the instance's time range, and the sequence owns the spawnable, there's no reason to evaluate - it should already be destroyed
	if (Spawnable && Spawnable->GetSpawnOwnership() == ESpawnOwnership::InnerSequence && !Range.Contains(UpdateData.Position))
	{
		bool bDestroy = true;
#if WITH_EDITORONLY_DATA
		bDestroy = !Spawnable->ShouldIgnoreOwnershipInEditor();
		// Don't destroy cameras while previewing
		if (bIsPreview && MovieSceneHelpers::CameraComponentFromActor(Cast<AActor>(Spawnable->GetObjectTemplate())))
		{
			bDestroy = false;
		}
#endif
		if (bDestroy)
		{
			SpawnRegister.DestroySpawnedObject(Track->GetObjectId(), SequenceInstance, Player);
			return;
		}
	}

	bool bIsSpawned = false;
	if (Track->Eval(UpdateData.Position, UpdateData.LastPosition, bIsSpawned))
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