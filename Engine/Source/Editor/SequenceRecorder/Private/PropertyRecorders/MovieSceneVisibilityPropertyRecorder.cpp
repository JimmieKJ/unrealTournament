// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "MovieSceneVisibilityPropertyRecorder.h"
#include "MovieSceneVisibilitySection.h"
#include "MovieSceneVisibilityTrack.h"
#include "MovieScene.h"

static const FName VisibilityTrackName = TEXT("Visibility");

void FMovieSceneVisibilityPropertyRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* MovieScene, const FGuid& Guid, float Time, bool bRecord)
{
	ObjectToRecord = InObjectToRecord;

	UMovieSceneVisibilityTrack* VisibilityTrack = MovieScene->AddTrack<UMovieSceneVisibilityTrack>(Guid);
	if(VisibilityTrack)
	{
		VisibilityTrack->SetPropertyNameAndPath(VisibilityTrackName, VisibilityTrackName.ToString());

		MovieSceneSection = Cast<UMovieSceneVisibilitySection>(VisibilityTrack->CreateNewSection());

		VisibilityTrack->AddSection(*MovieSceneSection);

		MovieSceneSection->SetDefault(false);

		bWasVisible = false;
		if(USceneComponent* SceneComponent = Cast<USceneComponent>(ObjectToRecord.Get()))
		{
			bWasVisible = SceneComponent->IsVisible() && SceneComponent->IsRegistered();
		}
		else if(AActor* Actor = Cast<AActor>(ObjectToRecord.Get()))
		{
			bWasVisible = !Actor->bHidden;
		}

		MovieSceneSection->AddKey(Time, bWasVisible, EMovieSceneKeyInterpolation::Break);

		MovieSceneSection->SetStartTime(Time);
	}

	bRecording = bRecord;

	Record(Time);
}

void FMovieSceneVisibilityPropertyRecorder::FinalizeSection()
{
	bRecording = false;
}

void FMovieSceneVisibilityPropertyRecorder::Record(float CurrentTime)
{
	if(ObjectToRecord.IsValid())
	{
		MovieSceneSection->SetEndTime(CurrentTime);

		if(bRecording)
		{
			bool bVisible = false;
			if(USceneComponent* SceneComponent = Cast<USceneComponent>(ObjectToRecord.Get()))
			{
				bVisible = SceneComponent->IsVisible() && SceneComponent->IsRegistered();
			}
			else if(AActor* Actor = Cast<AActor>(ObjectToRecord.Get()))
			{
				bVisible = !Actor->bHidden;
			}

			if(bVisible != bWasVisible)
			{
				MovieSceneSection->AddKey(CurrentTime, bVisible, EMovieSceneKeyInterpolation::Break);
			}
			bWasVisible = bVisible;
		}
	}
}