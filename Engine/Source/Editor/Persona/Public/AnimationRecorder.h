// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FAnimationRecorder

// records the mesh pose to animation input
struct PERSONA_API FAnimationRecorder
{
private:
	float IntervalTime;
	int32 MaxFrame;
	int32 LastFrame;
	float TimePassed;
	UAnimSequence * AnimationObject;
	TArray<FTransform> PreviousSpacesBases;

public:
	FAnimationRecorder();
	~FAnimationRecorder();

	// also provides interface
	bool TriggerRecordAnimation(USkeletalMeshComponent * Component);
	void StartRecord(USkeletalMeshComponent * Component, UAnimSequence * InAnimationObject);
	UAnimSequence * StopRecord(bool bShowMessage);
	void UpdateRecord(USkeletalMeshComponent * Component, float DeltaTime);
	UAnimSequence* GetAnimationObject() const { return AnimationObject; }
	bool InRecording() const { return AnimationObject != NULL; }
	float GetTimeRecorded() const { return TimePassed; }

	// If true, it will record root to include LocalToWorld
	bool bRecordLocalToWorld;

private:
	void Record( USkeletalMeshComponent * Component, TArray<FTransform> SpacesBases, int32 FrameToAdd );
};

