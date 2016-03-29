// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FAnimationRecorder

// records the mesh pose to animation input
struct FAnimationRecorder
{
private:
	/** Frame count used to signal an unbounded animation */
	static const int32 UnBoundedFrameCount = -1;

private:
	float IntervalTime;
	int32 MaxFrame;
	int32 LastFrame;
	float TimePassed;
	UAnimSequence* AnimationObject;
	TArray<FTransform> PreviousSpacesBases;
	FBlendedHeapCurve PreviousAnimCurves;
	FTransform PreviousComponentToWorld;

	/** Array of currently active notifies that have duration */
	TArray<TPair<const FAnimNotifyEvent*, bool>> ActiveNotifies;

	/** Unique notifies added to this sequence during recording */
	TMap<UAnimNotify*, UAnimNotify*> UniqueNotifies;

	/** Unique notify states added to this sequence during recording */
	TMap<UAnimNotifyState*, UAnimNotifyState*> UniqueNotifyStates;

	static float DefaultSampleRate;

public:
	FAnimationRecorder();
	~FAnimationRecorder();

	/** Starts recording an animation. Prompts for asset path and name via dialog if none provided */
	bool TriggerRecordAnimation(USkeletalMeshComponent* Component);
	bool TriggerRecordAnimation(USkeletalMeshComponent* Component, const FString& AssetPath, const FString& AssetName);

	void StartRecord(USkeletalMeshComponent* Component, UAnimSequence* InAnimationObject);
	UAnimSequence* StopRecord(bool bShowMessage);
	void UpdateRecord(USkeletalMeshComponent* Component, float DeltaTime);
	UAnimSequence* GetAnimationObject() const { return AnimationObject; }
	bool InRecording() const { return AnimationObject != nullptr; }
	float GetTimeRecorded() const { return TimePassed; }

	/** Sets a new sample rate & max length for this recorder. Don't call while recording. */
	void SetSampleRateAndLength(float SampleRateHz, float LengthInMinutes);

	bool SetAnimCompressionScheme(TSubclassOf<class UAnimCompress> SchemeClass);

	/** If true, it will record root to include LocalToWorld */
	uint8 bRecordLocalToWorld :1;
	/** If true, asset will be saved to disk after recording. If false, asset will remain in mem and can be manually saved. */
	uint8 bAutoSaveAsset : 1;

private:
	void Record(USkeletalMeshComponent* Component, FTransform const& ComponentToWorld, const TArray<FTransform>& SpacesBases, const FBlendedHeapCurve& AnimationCurves, int32 FrameToAdd);

	void RecordNotifies(USkeletalMeshComponent* Component, const TArray<const struct FAnimNotifyEvent*>& AnimNotifies, float DeltaTime, float RecordTime);

	void FixupNotifies();
};

//////////////////////////////////////////////////////////////////////////
// FAnimRecorderInstance

struct FAnimRecorderInstance
{
public:
	FAnimRecorderInstance();
	~FAnimRecorderInstance();

	void Init(USkeletalMeshComponent* InComponent, const FString& InAssetPath, const FString& InAssetName, float SampleRateHz, float MaxLength, bool bRecordInWorldSpace, bool bAutoSaveAsset);

	void Init(USkeletalMeshComponent* InComponent, UAnimSequence* InSequence, float SampleRateHz, float MaxLength, bool bRecordInWorldSpace, bool bAutoSaveAsset);

	bool BeginRecording();
	void Update(float DeltaTime);
	void FinishRecording(bool bShowMessage = true);

	TWeakObjectPtr<USkeletalMeshComponent> SkelComp;
	TWeakObjectPtr<UAnimSequence> Sequence;
	FString AssetPath;
	FString AssetName;

	/** Original ForcedLodModel setting on the SkelComp, so we can modify it and restore it when we are done. */
	int CachedSkelCompForcedLodModel;

	TSharedPtr<FAnimationRecorder> Recorder;
};


//////////////////////////////////////////////////////////////////////////
// FAnimationRecorderManager

struct FAnimationRecorderManager
{
public:
	/** Singleton accessor */
	static FAnimationRecorderManager& Get();

	/** Destructor */
	virtual ~FAnimationRecorderManager();

	/** Starts recording an animation. */
	bool RecordAnimation(USkeletalMeshComponent* Component, const FString& AssetPath = FString(), const FString& AssetName = FString(), const FAnimationRecordingSettings& Settings = FAnimationRecordingSettings());

	bool RecordAnimation(USkeletalMeshComponent* Component, UAnimSequence* Sequence, const FAnimationRecordingSettings& Settings = FAnimationRecordingSettings());

	bool IsRecording(USkeletalMeshComponent* Component);

	bool IsRecording();

	UAnimSequence* GetCurrentlyRecordingSequence(USkeletalMeshComponent* Component);
	float GetCurrentRecordingTime(USkeletalMeshComponent* Component);
	void StopRecordingAnimation(USkeletalMeshComponent* Component, bool bShowMessage = true);
	void StopRecordingAllAnimations();

	void Tick(float DeltaTime);

	void Tick(USkeletalMeshComponent* Component, float DeltaTime);

	void StopRecordingDeadAnimations(bool bShowMessage = true);

private:
	/** Constructor, private - use Get() function */
	FAnimationRecorderManager();

	TArray<FAnimRecorderInstance> RecorderInstances;

	void HandleEndPIE(bool bSimulating);
};

