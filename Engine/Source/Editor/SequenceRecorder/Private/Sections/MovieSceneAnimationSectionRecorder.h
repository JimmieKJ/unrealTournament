// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FMovieSceneAnimationSectionRecorderFactory : public IMovieSceneSectionRecorderFactory
{
public:
	virtual ~FMovieSceneAnimationSectionRecorderFactory() {}

	virtual bool CanRecordObject(class UObject* InObjectToRecord) const override;

	TSharedPtr<class FMovieSceneAnimationSectionRecorder> CreateSectionRecorder(class UActorRecording* InActorRecording, const FAnimationRecordingSettings& InAnimationSettings) const;

private:
	virtual TSharedPtr<IMovieSceneSectionRecorder> CreateSectionRecorder(const struct FActorRecordingSettings& InActorRecordingSettings) const override;
};

class FMovieSceneAnimationSectionRecorder : public IMovieSceneSectionRecorder
{
public:
	FMovieSceneAnimationSectionRecorder(const FAnimationRecordingSettings& InAnimationSettings, UAnimSequence* InSpecifiedSequence);
	virtual ~FMovieSceneAnimationSectionRecorder() {}

	virtual void CreateSection(UObject* InObjectToRecord, class UMovieScene* MovieScene, const FGuid& Guid, float Time) override;
	virtual void FinalizeSection() override;
	virtual void Record(float CurrentTime) override;
	virtual void InvalidateObjectToRecord() override
	{
		ObjectToRecord = nullptr;
	}
	virtual UObject* GetSourceObject() const override
	{
		return ObjectToRecord.Get();
	}

	void SetRemoveRootTransform(bool bInRemoveRootTransform) { bRemoveRootTransform = bInRemoveRootTransform; }

	UAnimSequence* GetAnimSequence() const { return AnimSequence.Get(); }

	USkeletalMesh* GetSkeletalMesh() const { return SkeletalMesh.Get(); }

	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent.Get(); }

	const FTransform& GetComponentTransform() const { return ComponentTransform; }

private:
	/** Object to record from */
	TLazyObjectPtr<UObject> ObjectToRecord;

	/** Section to record to */
	TWeakObjectPtr<class UMovieSceneSkeletalAnimationSection> MovieSceneSection;

	TWeakObjectPtr<class UAnimSequence> AnimSequence;

	TWeakObjectPtr<class USkeletalMeshComponent> SkeletalMeshComponent;

	TWeakObjectPtr<class USkeletalMesh> SkeletalMesh;

	bool bRemoveRootTransform;

	/** Local transform of the component we are recording */
	FTransform ComponentTransform;

	FAnimationRecordingSettings AnimationSettings;
};