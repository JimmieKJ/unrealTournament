// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieScenePropertyRecorder.h"

/** Structure used to buffer up transform keys. Keys are inserted into tracks in FinalizeSection() */
struct FBufferedTransformKey
{
	FBufferedTransformKey(const FTransform& InTransform, float InKeyTime)
		: Transform(InTransform)
		, WoundRotation(InTransform.Rotator())
		, KeyTime(InKeyTime)
	{
	}

	FTransform Transform;
	FRotator WoundRotation;
	float KeyTime;
};

class FMovieScene3DTransformPropertyRecorder : public IMovieScenePropertyRecorder
{
public:
	FMovieScene3DTransformPropertyRecorder(TSharedPtr<class FMovieSceneAnimationPropertyRecorder> InAnimRecorder = nullptr) 
		: AnimRecorder(InAnimRecorder)
	{}

	virtual ~FMovieScene3DTransformPropertyRecorder() {}

	virtual void CreateSection(UObject* InObjectToRecord, class UMovieScene* InMovieScene, const FGuid& Guid, float Time, bool bRecord) override;
	virtual void FinalizeSection() override;
	virtual void Record(float CurrentTime) override;
	virtual void InvalidateObjectToRecord() override
	{
		ObjectToRecord = nullptr;
	}

private:
	FTransform GetTransformToRecord();

private:
	/** Object to record from */
	TLazyObjectPtr<UObject> ObjectToRecord;

	/** MovieScene to record to */
	TWeakObjectPtr<class UMovieScene> MovieScene;

	/** Track to record to */
	TWeakObjectPtr<class UMovieScene3DTransformTrack> MovieSceneTrack;

	/** Section to record to */
	TWeakObjectPtr<class UMovieScene3DTransformSection> MovieSceneSection;

	/** Buffer of transform keys. Keys are inserted into tracks in FinalizeSection() */
	TArray<FBufferedTransformKey> BufferedTransforms;

	/** Flag if we are actually recording or not */
	bool bRecording;

	/** Animation recorder we use to sync our transforms to */
	TSharedPtr<class FMovieSceneAnimationPropertyRecorder> AnimRecorder;

	/** The default transform this recording starts with */
	FTransform DefaultTransform;

	/** Flag indicating that some time while this recorder was active an attachment was also in place */
	bool bWasAttached;
};