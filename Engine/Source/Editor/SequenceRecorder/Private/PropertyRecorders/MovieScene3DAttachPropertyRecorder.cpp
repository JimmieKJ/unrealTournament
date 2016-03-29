// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "MovieScene3DAttachPropertyRecorder.h"
#include "MovieScene3DAttachSection.h"
#include "MovieScene3DAttachTrack.h"
#include "MovieScene.h"
#include "SequenceRecorderUtils.h"
#include "SequenceRecorder.h"
#include "ActorRecording.h"

void FMovieScene3DAttachPropertyRecorder::CreateSection(UObject* InObjectToRecord, UMovieScene* InMovieScene, const FGuid& Guid, float Time, bool bRecord)
{
	ObjectGuid = Guid;
	ActorToRecord = CastChecked<AActor>(InObjectToRecord);
	MovieScene = InMovieScene;

	bRecording = bRecord;

	Record(Time);
}

void FMovieScene3DAttachPropertyRecorder::FinalizeSection()
{
	bRecording = false;
}

void FMovieScene3DAttachPropertyRecorder::Record(float CurrentTime)
{
	if(ActorToRecord.IsValid())
	{
		if(bRecording)
		{
			if(MovieSceneSection.IsValid())
			{
				MovieSceneSection->SetEndTime(CurrentTime);
			}

			// get attachment and check if the actor we are attached to is being recorded
			FName SocketName;
			FName ComponentName;
			AActor* AttachedToActor = SequenceRecorderUtils::GetAttachment(ActorToRecord.Get(), SocketName, ComponentName);
			UActorRecording* ActorRecording = FSequenceRecorder::Get().FindRecording(AttachedToActor);
			if(AttachedToActor && ActorRecording)
			{
				// create the track if we haven't already
				if(!AttachTrack.IsValid())
				{
					AttachTrack = MovieScene->AddTrack<UMovieScene3DAttachTrack>(ObjectGuid);
				}

				// check if we need a section or if the actor we are attached to has changed
				if(!MovieSceneSection.IsValid() || AttachedToActor != ActorAttachedTo.Get())
				{
					MovieSceneSection = Cast<UMovieScene3DAttachSection>(AttachTrack->CreateNewSection());
					MovieSceneSection->SetStartTime(CurrentTime);
					MovieSceneSection->SetConstraintId(ActorRecording->Guid);
					MovieSceneSection->AttachSocketName = SocketName;
					MovieSceneSection->AttachComponentName = ComponentName;
				}

				ActorAttachedTo = AttachedToActor;
			}
			else
			{
				// no attachment, so end the section recording if we have any
				MovieSceneSection = nullptr;
			}
		}
	}
}