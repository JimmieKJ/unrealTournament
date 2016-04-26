// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieScenePropertyRecorder.h"

class FMovieSceneVisibilityPropertyRecorder : public IMovieScenePropertyRecorder
{
public:
	virtual ~FMovieSceneVisibilityPropertyRecorder() {}

	virtual void CreateSection(UObject* InObjectToRecord, class UMovieScene* MovieScene, const FGuid& Guid, float Time, bool bRecord) override;
	virtual void FinalizeSection() override;
	virtual void Record(float CurrentTime) override;
	virtual void InvalidateObjectToRecord() override
	{
		ObjectToRecord = nullptr;
	}

private:
	/** Object to record from */
	TLazyObjectPtr<class UObject> ObjectToRecord;

	/** Section to record to */
	TWeakObjectPtr<class UMovieSceneVisibilitySection> MovieSceneSection;

	/** Flag if we are actually recording or not */
	bool bRecording;

	/** Flag used to track visibility state and add keys when this changes */
	bool bWasVisible;
};