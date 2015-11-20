// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FAnimationRecorder

// records the mesh pose to animation input
struct UNREALED_API FAnimationRecorder
{
private:
	float IntervalTime;
	int32 MaxFrame;
	int32 LastFrame;
	float TimePassed;
	UAnimSequence* AnimationObject;
	TArray<FTransform> PreviousSpacesBases;
	FTransform PreviousComponentToWorld;

	static float DefaultSampleRate;

public:
	FAnimationRecorder();
	~FAnimationRecorder();

	/** Starts recording an animation. Prompts for asset path and name via dialog. */
	bool TriggerRecordAnimation(USkeletalMeshComponent* Component);
	/** Starts recording an animation. */
	bool TriggerRecordAnimation(USkeletalMeshComponent* Component, FString AssetPath, FString AssetName);

	void StartRecord(USkeletalMeshComponent* Component, UAnimSequence* InAnimationObject);
	UAnimSequence* StopRecord(bool bShowMessage);
	void UpdateRecord(USkeletalMeshComponent* Component, float DeltaTime);
	UAnimSequence* GetAnimationObject() const { return AnimationObject; }
	bool InRecording() const { return AnimationObject != nullptr; }
	float GetTimeRecorded() const { return TimePassed; }

	/** Sets a new sample rate for this recorder. Don't call while recording. */
	void SetSampleRate(float SampleRateHz);

	bool SetAnimCompressionScheme(TSubclassOf<class UAnimCompress> SchemeClass);

	/** If true, it will record root to include LocalToWorld */
	uint8 bRecordLocalToWorld :1;
	/** If true, asset will be saved to disk after recording. If false, asset will remain in mem and can be manually saved. */
	uint8 bAutoSaveAsset : 1;

private:
	void Record(USkeletalMeshComponent* Component, FTransform const& ComponentToWorld, TArray<FTransform> SpacesBases, int32 FrameToAdd);
};

//////////////////////////////////////////////////////////////////////////
// FAnimRecorderInstance

struct FAnimRecorderInstance
{
public:
	FAnimRecorderInstance();
	~FAnimRecorderInstance();

	void Init(AActor* InActor, USkeletalMeshComponent* InComponent, FString InAssetPath, FString InAssetName, float SampleRateHz, bool bRecordInWorldSpace);

	bool BeginRecording();
	void Update(float DeltaTime);
	void FinishRecording();

	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<USkeletalMeshComponent> SkelComp;
	FString AssetPath;
	FString AssetName;

	/** Original ForcedLodModel setting on the SkelComp, so we can modify it and restore it when we are done. */
	int CachedSkelCompForcedLodModel;

	FAnimationRecorder* Recorder;
};


//////////////////////////////////////////////////////////////////////////
// FAnimationRecorderManager

struct UNREALED_API FAnimationRecorderManager
{
public:
	/** Singleton accessor */
	static FAnimationRecorderManager& Get();

	/** Destructor */
	virtual ~FAnimationRecorderManager();

	/** Starts recording an animation. */
	bool RecordAnimation(AActor* Actor, USkeletalMeshComponent* Component, FString AssetPath, FString AssetName);
	void StopRecordingAnimation(AActor* Actor, USkeletalMeshComponent* Component);
	void StopRecordingAllAnimations();

	void Tick(float DeltaTime);

private:
	/** Constructor, private - use Get() function */
	FAnimationRecorderManager();

	TArray<FAnimRecorderInstance> RecorderInstances;
};

