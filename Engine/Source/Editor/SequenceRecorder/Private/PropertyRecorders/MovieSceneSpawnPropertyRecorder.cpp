// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "MovieSceneSpawnPropertyRecorder.h"
#include "MovieSceneBoolSection.h"
#include "MovieSceneSpawnTrack.h"
#include "MovieScene.h"

void FMovieSceneSpawnPropertyRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* MovieScene, const FGuid& Guid, float Time, bool bRecord)
{
	ObjectToRecord = InObjectToRecord;

	UMovieSceneSpawnTrack* SpawnTrack = MovieScene->AddTrack<UMovieSceneSpawnTrack>(Guid);
	if(SpawnTrack)
	{
		MovieSceneSection = Cast<UMovieSceneBoolSection>(SpawnTrack->CreateNewSection());

		SpawnTrack->AddSection(*MovieSceneSection);
		SpawnTrack->SetObjectId(Guid);

		MovieSceneSection->SetDefault(false);
		MovieSceneSection->AddKey(0.0f, false, EMovieSceneKeyInterpolation::Break);

		MovieSceneSection->SetStartTime(Time);
		MovieSceneSection->SetIsInfinite(true);
	}

	bRecording = bRecord;

	bWasSpawned = false;

	Record(Time);
}

void FMovieSceneSpawnPropertyRecorder::FinalizeSection()
{
	bRecording = false;

	const bool bSpawned = ObjectToRecord.IsValid();
	if(bSpawned != bWasSpawned)
	{
		MovieSceneSection->AddKey(MovieSceneSection->GetEndTime(), bSpawned, EMovieSceneKeyInterpolation::Break);
	}
}

void FMovieSceneSpawnPropertyRecorder::Record(float CurrentTime)
{
	if(bRecording)
	{
		if(ObjectToRecord.IsValid())
		{
			MovieSceneSection->SetEndTime(CurrentTime);
		}

		const bool bSpawned = ObjectToRecord.IsValid();
		if(bSpawned != bWasSpawned)
		{
			MovieSceneSection->AddKey(CurrentTime, bSpawned, EMovieSceneKeyInterpolation::Break);
		}
		bWasSpawned = bSpawned;
	}
}