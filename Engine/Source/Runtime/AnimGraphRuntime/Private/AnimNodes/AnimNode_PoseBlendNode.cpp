// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimNodes/AnimNode_PoseBlendNode.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimPoseByNameNode

void FAnimNode_PoseBlendNode::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_PoseHandler::Initialize(Context);

	SourcePose.Initialize(Context);
}

void FAnimNode_PoseBlendNode::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	FAnimNode_PoseHandler::CacheBones(Context);
	SourcePose.CacheBones(Context);
}

void FAnimNode_PoseBlendNode::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	FAnimNode_PoseHandler::UpdateAssetPlayer(Context);
	SourcePose.Update(Context);
}

void FAnimNode_PoseBlendNode::Evaluate(FPoseContext& Output)
{
	FPoseContext SourceData(Output);
	SourcePose.Evaluate(SourceData);

	if (CurrentPoseAsset.IsValid() && (Output.AnimInstanceProxy->IsSkeletonCompatible(CurrentPoseAsset->GetSkeleton())))
	{
		FPoseContext CurrentPose(Output);
		check(PoseExtractContext.PoseCurves.Num() == PoseUIDList.Num());
		// only give pose curve, we don't set any more curve here
		for (int32 PoseIdx = 0; PoseIdx < PoseUIDList.Num(); ++PoseIdx)
		{
			PoseExtractContext.PoseCurves[PoseIdx] = SourceData.Curve.Get(PoseUIDList[PoseIdx]);
		}

		if (CurrentPoseAsset.Get()->GetAnimationPose(CurrentPose.Pose, CurrentPose.Curve, PoseExtractContext))
		{
			// once we get it, we have to blend by weight
			if (CurrentPoseAsset->IsValidAdditive())
			{
				Output = SourceData;
				FAnimationRuntime::AccumulateAdditivePose(Output.Pose, CurrentPose.Pose, Output.Curve, CurrentPose.Curve, 1.f, EAdditiveAnimationType::AAT_LocalSpaceBase);
			}
			else
			{
				FAnimationRuntime::BlendTwoPosesTogetherPerBone(SourceData.Pose, CurrentPose.Pose, SourceData.Curve, CurrentPose.Curve, BoneBlendWeights, Output.Pose, Output.Curve);
			}
		}
	}
	else
	{
		Output = SourceData;
	}
}

void FAnimNode_PoseBlendNode::GatherDebugData(FNodeDebugData& DebugData)
{
	FAnimNode_PoseHandler::GatherDebugData(DebugData);
	SourcePose.GatherDebugData(DebugData.BranchFlow(1.f));
}

