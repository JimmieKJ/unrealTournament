// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "MovieSceneParticleTrackPropertyRecorder.h"
#include "MovieSceneParticleSection.h"
#include "MovieSceneParticleTrack.h"
#include "MovieScene.h"
#include "Particles/ParticleSystemComponent.h"

FMovieSceneParticleTrackPropertyRecorder::~FMovieSceneParticleTrackPropertyRecorder()
{
	if(DelegateProxy.IsValid())
	{
		DelegateProxy->PropertyRecorder = nullptr;
		DelegateProxy->RemoveFromRoot();
		DelegateProxy.Reset();
	}
}

void FMovieSceneParticleTrackPropertyRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* MovieScene, const FGuid& Guid, float Time, bool bRecord)
{
	SystemToRecord = CastChecked<UParticleSystemComponent>(InObjectToRecord);

	UMovieSceneParticleTrack* ParticleTrack = MovieScene->AddTrack<UMovieSceneParticleTrack>(Guid);
	if(ParticleTrack)
	{
		MovieSceneSection = Cast<UMovieSceneParticleSection>(ParticleTrack->CreateNewSection());

		ParticleTrack->AddSection(*MovieSceneSection);

		MovieSceneSection->SetStartTime(Time);

		bWasTriggered = false;

		DelegateProxy = NewObject<UMovieSceneParticleTrackPropertyRecorder>();
		DelegateProxy->PropertyRecorder = this;
		DelegateProxy->AddToRoot();
		UParticleSystemComponent::OnSystemPreActivationChange.AddUObject(DelegateProxy.Get(), &UMovieSceneParticleTrackPropertyRecorder::OnTriggered);
	}

	bRecording = bRecord;

	PreviousState = EParticleKey::Deactivate;

	Record(Time);
}

void FMovieSceneParticleTrackPropertyRecorder::FinalizeSection()
{
	bRecording = false;
}

void FMovieSceneParticleTrackPropertyRecorder::Record(float CurrentTime)
{
	if(SystemToRecord.IsValid())
	{
		MovieSceneSection->SetEndTime(CurrentTime);

		if(bRecording)
		{
			EParticleKey::Type NewState = EParticleKey::Deactivate;
			if(SystemToRecord->IsRegistered() && SystemToRecord->IsActive() && !SystemToRecord->bWasDeactivated)
			{
				if(bWasTriggered)
				{
					NewState = EParticleKey::Trigger;
					bWasTriggered = false;
				}
				else
				{
					NewState = EParticleKey::Activate;
				}
			}
			else
			{
				NewState = EParticleKey::Deactivate;
			}

			if(NewState != PreviousState)
			{
				MovieSceneSection->AddKey(CurrentTime, NewState);
			}

			if(NewState == EParticleKey::Trigger)
			{
				NewState = EParticleKey::Activate;
			}
			PreviousState = NewState;
		}
	}
}

void UMovieSceneParticleTrackPropertyRecorder::OnTriggered(UParticleSystemComponent* Component, bool bActivating)
{ 
	if(PropertyRecorder && PropertyRecorder->SystemToRecord.Get() == Component)
	{
		PropertyRecorder->bWasTriggered = bActivating;
	}
}