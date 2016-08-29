// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderSectionsPrivatePCH.h"
#include "MovieSceneVisibilitySectionRecorder.h"
#include "MovieSceneVisibilitySection.h"
#include "MovieSceneVisibilityTrack.h"
#include "MovieScene.h"
#include "ActorRecordingSettings.h"

static const FName VisibilityTrackName = TEXT("Visibility");

TSharedPtr<IMovieSceneSectionRecorder> FMovieSceneVisibilitySectionRecorderFactory::CreateSectionRecorder(const struct FActorRecordingSettings& InActorRecordingSettings) const
{
	UMovieSceneVisibilitySectionRecorderSettings* Settings = InActorRecordingSettings.GetSettingsObject<UMovieSceneVisibilitySectionRecorderSettings>();
	check(Settings);
	if (Settings->bRecordVisibility)
	{
		return MakeShareable(new FMovieSceneVisibilitySectionRecorder);
	}

	return nullptr;
}

bool FMovieSceneVisibilitySectionRecorderFactory::CanRecordObject(UObject* InObjectToRecord) const
{
	return InObjectToRecord->IsA<AActor>() || InObjectToRecord->IsA<USceneComponent>();
}

void FMovieSceneVisibilitySectionRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* MovieScene, const FGuid& Guid, float Time)
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

		// if time is not at the very start of the movie scene, make sure 
		// we are hidden by default as the track will extrapolate backwards and show 
		// objects that shouldnt be visible.
		if (Time != MovieScene->GetPlaybackRange().GetLowerBoundValue())
		{
			MovieSceneSection->AddKey(MovieScene->GetPlaybackRange().GetLowerBoundValue(), false, EMovieSceneKeyInterpolation::Break);
		}

		MovieSceneSection->AddKey(Time, bWasVisible, EMovieSceneKeyInterpolation::Break);

		MovieSceneSection->SetStartTime(Time);
	}
}

void FMovieSceneVisibilitySectionRecorder::FinalizeSection()
{
}

void FMovieSceneVisibilitySectionRecorder::Record(float CurrentTime)
{
	if(ObjectToRecord.IsValid())
	{
		MovieSceneSection->SetEndTime(CurrentTime);

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