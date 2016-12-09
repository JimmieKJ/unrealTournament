// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_RotationOffsetBlendSpace.h"
#include "AnimationRuntime.h"

/////////////////////////////////////////////////////
// FAnimNode_RotationOffsetBlendSpace

void FAnimNode_RotationOffsetBlendSpace::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_BlendSpacePlayer::Initialize(Context);
	BasePose.Initialize(Context);
}

void FAnimNode_RotationOffsetBlendSpace::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	FAnimNode_BlendSpacePlayer::CacheBones(Context);
	BasePose.CacheBones(Context);
}

void FAnimNode_RotationOffsetBlendSpace::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	bIsLODEnabled = IsLODEnabled(Context.AnimInstanceProxy, LODThreshold);
	if (bIsLODEnabled)
	{
		FAnimNode_BlendSpacePlayer::UpdateAssetPlayer(Context);
	}

	BasePose.Update(Context);
}

void FAnimNode_RotationOffsetBlendSpace::Evaluate(FPoseContext& Context)
{
	// Evaluate base pose
	BasePose.Evaluate(Context);

	if (bIsLODEnabled)
	{
		// Evaluate MeshSpaceRotation additive blendspace
		FPoseContext MeshSpaceRotationAdditivePoseContext(Context);
		FAnimNode_BlendSpacePlayer::Evaluate(MeshSpaceRotationAdditivePoseContext);

		// Accumulate poses together
		FAnimationRuntime::AccumulateMeshSpaceRotationAdditiveToLocalPose(Context.Pose, MeshSpaceRotationAdditivePoseContext.Pose, Context.Curve, MeshSpaceRotationAdditivePoseContext.Curve, 1.f);

		// Resulting rotations are not normalized, so normalize here.
		Context.Pose.NormalizeRotations();
	}
}

void FAnimNode_RotationOffsetBlendSpace::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("(Play Time: %.3f)"), InternalTimeAccumulator);
	DebugData.AddDebugItem(DebugLine);
	
	BasePose.GatherDebugData(DebugData);
}

FAnimNode_RotationOffsetBlendSpace::FAnimNode_RotationOffsetBlendSpace()
	: LODThreshold(INDEX_NONE)
{
}
