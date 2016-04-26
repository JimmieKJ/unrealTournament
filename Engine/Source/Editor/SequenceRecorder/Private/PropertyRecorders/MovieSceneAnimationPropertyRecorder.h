// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieScenePropertyRecorder.h"

class FMovieSceneAnimationPropertyRecorder : public IMovieScenePropertyRecorder
{
public:
	FMovieSceneAnimationPropertyRecorder(FAnimationRecordingSettings& InAnimationSettings, UAnimSequence* InSpecifiedSequence);
	virtual ~FMovieSceneAnimationPropertyRecorder() {}

	virtual void CreateSection(UObject* InObjectToRecord, class UMovieScene* MovieScene, const FGuid& Guid, float Time, bool bRecord) override;
	virtual void FinalizeSection() override;
	virtual void Record(float CurrentTime) override;
	virtual void InvalidateObjectToRecord() override
	{
		ObjectToRecord = nullptr;
	}

	UAnimSequence* GetSequence() const { return Sequence.Get(); }

	USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh.Get(); }

	const FTransform& GetComponentTransform() const { return ComponentTransform; }

private:
	/** Object to record from */
	TLazyObjectPtr<UObject> ObjectToRecord;

	/** Section to record to */
	TWeakObjectPtr<class UMovieSceneSkeletalAnimationSection> MovieSceneSection;

	TWeakObjectPtr<class UAnimSequence> Sequence;

	TWeakObjectPtr<class USkeletalMeshComponent> SkeletalMeshComponent;

	TWeakObjectPtr<class USkeletalMesh> SkeletalMesh;

	/** Local transform of the component we are recording */
	FTransform ComponentTransform;

	FAnimationRecordingSettings AnimationSettings;

	/** Used to store/restore update flag when recording */
	EMeshComponentUpdateFlag::Type MeshComponentUpdateFlag;

	/** Used to store/restore URO when recording */
	bool bEnableUpdateRateOptimizations;

	/** Flag if we are actually recording or not */
	bool bRecording;
};