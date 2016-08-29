// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/AnimNodeBase.h"
#include "Animation/PoseAsset.h"
#include "AnimNode_PoseHandler.h"
#include "AnimNode_PoseByName.generated.h"

// Evaluates a point in an anim sequence, using a specific time input rather than advancing time internally.
// Typically the playback position of the animation for this node will represent something other than time, like jump height.
// This node will not trigger any notifies present in the associated sequence.
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_PoseByName : public FAnimNode_PoseHandler
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	FName PoseName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PinHiddenByDefault))
	float PoseWeight;

public:	
	FAnimNode_PoseByName()
		: PoseWeight(1.f)
		, PoseUID(FSmartNameMapping::MaxUID)
	{
	}

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface
private:
	// PoseUID
	FSmartNameMapping::UID PoseUID;
};

