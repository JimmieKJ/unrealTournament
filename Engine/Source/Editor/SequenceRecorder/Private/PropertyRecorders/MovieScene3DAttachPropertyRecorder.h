// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieScenePropertyRecorder.h"

class FMovieScene3DAttachPropertyRecorder : public IMovieScenePropertyRecorder
{
public:
	virtual ~FMovieScene3DAttachPropertyRecorder() {}

	virtual void CreateSection(UObject* InObjectToRecord, class UMovieScene* InMovieScene, const FGuid& Guid, float Time, bool bRecord) override;
	virtual void FinalizeSection() override;
	virtual void Record(float CurrentTime) override;
	virtual void InvalidateObjectToRecord() override
	{
		ActorToRecord = nullptr;
	}

private:
	/** Object to record from */
	TLazyObjectPtr<class AActor> ActorToRecord;

	/** Section to record to */
	TWeakObjectPtr<class UMovieScene3DAttachSection> MovieSceneSection;

	/** Track we are recording to */
	TWeakObjectPtr<class UMovieScene3DAttachTrack> AttachTrack;

	/** Movie scene we are recording to */
	TWeakObjectPtr<class UMovieScene> MovieScene;

	/** Track the actor we are attached to */
	TLazyObjectPtr<class AActor> ActorAttachedTo;

	/** Identifier of the object we are recording */
	FGuid ObjectGuid;

	/** Flag if we are actually recording or not */
	bool bRecording;
};