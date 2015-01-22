// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNodeBase.h"
#include "AnimNode_BlendListBase.generated.h"

// Blend list node; has many children
USTRUCT()
struct ENGINE_API FAnimNode_BlendListBase : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category=Links)
	TArray<FPoseLink> BlendPose;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category=Config, meta=(PinShownByDefault))
	TArray<float> BlendTime;

protected:
	UPROPERTY()
	TArray<float> BlendWeights;

	UPROPERTY()
	TArray<float> RemainingBlendTimes;

	UPROPERTY()
	int32 LastActiveChildIndex;
public:	
	FAnimNode_BlendListBase()
		: LastActiveChildIndex(0)
	{
		BlendWeights.Empty();
		RemainingBlendTimes.Empty();
	}

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

#if WITH_EDITOR
	virtual void AddPose()
	{
		BlendTime.Add(0.1f);
		new (BlendPose) FPoseLink();
	}

	virtual void RemovePose(int32 PoseIndex)
	{
		BlendTime.RemoveAt(PoseIndex);
		BlendPose.RemoveAt(PoseIndex);
	}
#endif

protected:
	virtual int32 GetActiveChildIndex() { return 0; }
	virtual FString GetNodeName(FNodeDebugData& DebugData) { return DebugData.GetNodeName(this); }
};
