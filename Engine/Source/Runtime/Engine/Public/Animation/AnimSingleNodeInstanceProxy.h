// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	}

	FAnimSingleNodeInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
		, CurrentAsset(nullptr)
		, CurrentVertexAnim(nullptr)
		, BlendSpaceInput(0.0f, 0.0f, 0.0f)
		, CurrentTime(0.0f)
#if WITH_EDITORONLY_DATA
		, PreviewPoseCurrentTime(0.0f)
#endif
		, PlayRate(1.f)
		, bLooping(true)
		, bPlaying(true)
		, bReverse(false)
	{}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void UpdateAnimationNode(float DeltaSeconds) override;
	virtual void PostUpdate(UAnimInstance* InAnimInstance) const override;

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

	UAnimationAsset* GetCurrentAsset() { return CurrentAsset; }

	UVertexAnimation* GetCurrentVertexAnimation() { return CurrentVertexAnim; }

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

	void SetVertexAnimation(UVertexAnimation * NewVertexAnim, bool bIsLooping, float InPlayRate);

	void SetReverse(bool bInReverse);

	float GetLength();

	void SetBlendSpaceInput(const FVector& InBlendInput);

private:
	void InternalBlendSpaceEvaluatePose(class UBlendSpaceBase* BlendSpace, TArray<FBlendSampleData>& BlendSampleDataCache, FPoseContext& OutContext);

private:
	/** Current Asset being played **/
	UAnimationAsset* CurrentAsset;

	/** Current vertex anim being played **/
	UVertexAnimation* CurrentVertexAnim;

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