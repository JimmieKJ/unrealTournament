// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimInstanceProxy.h"
#include "AnimSingleNodeInstanceProxy.generated.h"

/** Proxy override for this UAnimInstance-derived class */
USTRUCT()
struct ENGINE_API FAnimSingleNodeInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

public:
	FAnimSingleNodeInstanceProxy()
	{
#if WITH_EDITOR
		bCanProcessAdditiveAnimations = false;
#endif
	}

	FAnimSingleNodeInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
		, CurrentAsset(nullptr)
		, BlendSpaceInput(0.0f, 0.0f, 0.0f)
		, CurrentTime(0.0f)
#if WITH_EDITORONLY_DATA
		, PreviewPoseCurrentTime(0.0f)
#endif
		, PlayRate(1.f)
		, bLooping(true)
		, bPlaying(true)
		, bReverse(false)
	{
#if WITH_EDITOR
		bCanProcessAdditiveAnimations = false;
#endif
	}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void UpdateAnimationNode(float DeltaSeconds) override;
	virtual void PostUpdate(UAnimInstance* InAnimInstance) const override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void InitializeObjects(UAnimInstance* InAnimInstance) override;
	virtual void ClearObjects() override;

	void SetPlaying(bool bIsPlaying)
	{
		bPlaying = bIsPlaying;
	}

	bool IsPlaying() const
	{
		return bPlaying;
	}

	bool IsReverse() const
	{
		return bReverse;
	}

	void SetLooping(bool bIsLooping)
	{
		bLooping = bIsLooping;
	}

	bool IsLooping() const
	{
		return bLooping;
	}

	void ResetWeightInfo()
	{
		WeightInfo.Reset();
	}

	virtual void SetAnimationAsset(UAnimationAsset* NewAsset, USkeletalMeshComponent* MeshComponent, bool bIsLooping, float InPlayRate);

	void UpdateBlendspaceSamples(FVector InBlendInput);

	void SetCurrentTime(float InCurrentTime)
	{
		CurrentTime = InCurrentTime;
	}

	float GetCurrentTime() const
	{
		return CurrentTime;
	}

	float GetPlayRate() const
	{
		return PlayRate;
	}

	void SetPlayRate(float InPlayRate)
	{
		PlayRate = InPlayRate;
	}

	FVector GetFilterLastOutput()
	{
		return BlendFilter.GetFilterLastOutput();
	}

	void SetReverse(bool bInReverse);

	void SetBlendSpaceInput(const FVector& InBlendInput);

#if WITH_EDITOR
	bool CanProcessAdditiveAnimations() const
	{
		return bCanProcessAdditiveAnimations;
	}
#endif

#if WITH_EDITORONLY_DATA
	void PropagatePreviewCurve(FPoseContext& Output);
#endif // WITH_EDITORONLY_DATA

	void SetPreviewCurveOverride(const FName& PoseName, float Value, bool bRemoveIfZero);

private:
	void InternalBlendSpaceEvaluatePose(class UBlendSpaceBase* BlendSpace, TArray<FBlendSampleData>& BlendSampleDataCache, FPoseContext& OutContext);

protected:
#if WITH_EDITOR
	/** If this is being used by a user (for instance on a skeletal mesh placed in a level) we don't want to allow
	additives. But we need to be able to override this for editor preview windows */
	bool bCanProcessAdditiveAnimations;
#endif

	/** Pose Weight value that can override curve data. In the future, we'd like to have UCurveSet that can play by default**/
	TMap<FName, float> PreviewCurveOverride;

	/** Current Asset being played. Note that this will be nullptr outside of pre/post update **/
	UAnimationAsset* CurrentAsset;

private:
	/** Random cached values to play each asset **/
	FVector BlendSpaceInput;

	/** Random cached values to play each asset **/
	TArray<FBlendSampleData> BlendSampleData;

	/** Random cached values to play each asset **/
	FBlendFilter BlendFilter;

	/** Slot node weight transient data */
	FSlotNodeWeightInfo WeightInfo;

	/** Shared parameters for previewing blendspace or animsequence **/
	float CurrentTime;

#if WITH_EDITORONLY_DATA
	float PreviewPoseCurrentTime;
#endif

	/** Cache for data needed during marker sync */
	FMarkerTickRecord MarkerTickRecord;

	float PlayRate;

	uint32 bLooping:1;

	uint32 bPlaying:1;

	uint32 bReverse:1;
};