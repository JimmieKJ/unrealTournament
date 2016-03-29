// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_SkeletalControlBase.h"
#include "AnimNode_CopyPoseFromMesh.generated.h"

/**
 *	Simple controller to copy a bone's transform to another one.
 */

USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_CopyPoseFromMesh : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Copy, meta=(PinShownByDefault))
	USkeletalMeshComponent* SourceMeshComponent;

	FAnimNode_CopyPoseFromMesh();

	// FAnimNode_Base interface
	virtual void Initialize(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones(const FAnimationCacheBonesContext& Context) override;
	virtual void Update(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

private:
	TWeakObjectPtr<USkeletalMeshComponent> CurrentlyUsedSourceMeshComponent;
	// cache of target space bases to source space bases
	TMap<int32, int32> BoneMapToSource;

	// reinitialize mesh component 
	void ReinitializeMeshComponent(FAnimInstanceProxy* AnimInstanceProxy);
};
