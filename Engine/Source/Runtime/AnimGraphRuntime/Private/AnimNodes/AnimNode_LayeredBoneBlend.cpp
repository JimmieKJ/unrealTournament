// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimNodes/AnimNode_LayeredBoneBlend.h"
#include "AnimationRuntime.h"
#include "Animation/AnimStats.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_LayeredBoneBlend

void FAnimNode_LayeredBoneBlend::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize(Context);

	const int NumPoses = BlendPoses.Num();
	checkSlow(BlendWeights.Num() == NumPoses);

	// initialize children
	BasePose.Initialize(Context);

	if ( NumPoses > 0 )
	{
		for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
		{
			BlendPoses[ChildIndex].Initialize(Context);
		}

		// initialize mask weight now
		check (Context.AnimInstanceProxy->GetSkeleton());
		ReinitializeBoneBlendWeights(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());
	}
}

void FAnimNode_LayeredBoneBlend::ReinitializeBoneBlendWeights(const FBoneContainer& RequiredBones, const USkeleton* Skeleton)
{
	FAnimationRuntime::CreateMaskWeights(DesiredBoneBlendWeights, LayerSetup, RequiredBones, Skeleton);

	CurrentBoneBlendWeights.Empty(DesiredBoneBlendWeights.Num());
	CurrentBoneBlendWeights.AddZeroed(DesiredBoneBlendWeights.Num());
}

void FAnimNode_LayeredBoneBlend::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	BasePose.CacheBones(Context);
	for(int32 ChildIndex=0; ChildIndex<BlendPoses.Num(); ChildIndex++)
	{
		BlendPoses[ChildIndex].CacheBones(Context);
	}

	ReinitializeBoneBlendWeights(Context.AnimInstanceProxy->GetRequiredBones(), Context.AnimInstanceProxy->GetSkeleton());
}

void FAnimNode_LayeredBoneBlend::Update(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	bHasRelevantPoses = false;
	int32 RootMotionBlendPose = -1;
	float RootMotionWeight = 0.f;
	const float RootMotionClearWeight = bBlendRootMotionBasedOnRootBone ? 0.f : 1.f;

	for (int32 ChildIndex = 0; ChildIndex < BlendPoses.Num(); ++ChildIndex)
	{
		const float ChildWeight = BlendWeights[ChildIndex];
		if (FAnimWeight::IsRelevant(ChildWeight))
		{
			if (bHasRelevantPoses == false)
			{
				FAnimationRuntime::UpdateDesiredBoneWeight(DesiredBoneBlendWeights, CurrentBoneBlendWeights, BlendWeights);
				bHasRelevantPoses = true;

				if(bBlendRootMotionBasedOnRootBone)
				{
					const float NewRootMotionWeight = CurrentBoneBlendWeights[0].BlendWeight;
					if(NewRootMotionWeight > ZERO_ANIMWEIGHT_THRESH)
					{
						RootMotionWeight = NewRootMotionWeight;
						RootMotionBlendPose = CurrentBoneBlendWeights[0].SourceIndex;
					}
				}
			}

			const float ThisPoseRootMotionWeight = (ChildIndex == RootMotionBlendPose) ? RootMotionWeight : RootMotionClearWeight;
			BlendPoses[ChildIndex].Update(Context.FractionalWeightAndRootMotion(ChildWeight, ThisPoseRootMotionWeight));
		}
	}

	// initialize children
	const float BaseRootMotionWeight = 1.f - RootMotionWeight;

	if (BaseRootMotionWeight < ZERO_ANIMWEIGHT_THRESH)
	{
		BasePose.Update(Context.FractionalWeightAndRootMotion(1.f, BaseRootMotionWeight));
	}
	else
	{
		BasePose.Update(Context);
	}
}

void FAnimNode_LayeredBoneBlend::Evaluate(FPoseContext& Output)
{
	ANIM_MT_SCOPE_CYCLE_COUNTER(BlendPosesInGraph, !IsInGameThread());

	const int NumPoses = BlendPoses.Num();
	if ((NumPoses == 0) || !bHasRelevantPoses)
	{
		BasePose.Evaluate(Output);
	}
	else
	{
		FPoseContext BasePoseContext(Output);

		// evaluate children
		BasePose.Evaluate(BasePoseContext);

		TArray<FCompactPose> TargetBlendPoses;
		TargetBlendPoses.SetNum(NumPoses);

		TArray<FBlendedCurve> TargetBlendCurves;
		TargetBlendCurves.SetNum(NumPoses);

		for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
		{
			if (BlendWeights[ChildIndex] > ZERO_ANIMWEIGHT_THRESH)
			{
				FPoseContext CurrentPoseContext(Output);
				BlendPoses[ChildIndex].Evaluate(CurrentPoseContext);

				TargetBlendPoses[ChildIndex].CopyBonesFrom(CurrentPoseContext.Pose);
				TargetBlendCurves[ChildIndex].CopyFrom(CurrentPoseContext.Curve);
			}
			else
			{
				TargetBlendPoses[ChildIndex].ResetToRefPose(BasePoseContext.Pose.GetBoneContainer());
				TargetBlendCurves[ChildIndex].InitFrom(Output.Curve);
			}
		}

		FAnimationRuntime::BlendPosesPerBoneFilter(BasePoseContext.Pose, TargetBlendPoses, BasePoseContext.Curve, TargetBlendCurves, Output.Pose, Output.Curve, CurrentBoneBlendWeights, bMeshSpaceRotationBlend, CurveBlendOption);
	}
}


void FAnimNode_LayeredBoneBlend::GatherDebugData(FNodeDebugData& DebugData)
{
	const int NumPoses = BlendPoses.Num();

	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Num Poses: %i)"), NumPoses);
	DebugData.AddDebugItem(DebugLine);

	BasePose.GatherDebugData(DebugData.BranchFlow(1.f));
	
	for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
	{
		BlendPoses[ChildIndex].GatherDebugData(DebugData.BranchFlow(BlendWeights[ChildIndex]));
	}
}

#if WITH_EDITOR
void FAnimNode_LayeredBoneBlend::ValidateData()
{
	// ideally you don't like to get to situation where it becomes inconsistent, but this happened, 
	// and we don't know what caused this. Possibly copy/paste, but I tried copy/paste and that didn't work
	// so here we add code to fix this up manually in editor, so that they can continue working on it. 
	int32 PoseNum = BlendPoses.Num();
	int32 WeightNum = BlendWeights.Num();
	int32 LayerNum = LayerSetup.Num();

	int32 Max = FMath::Max3(PoseNum, WeightNum, LayerNum);
	int32 Min = FMath::Min3(PoseNum, WeightNum, LayerNum);
	// if they are not all same
	if (Min != Max)
	{
		// we'd like to increase to all Max
		// sadly we don't have add X for how many
		for (int32 Index=PoseNum; Index<Max; ++Index)
		{
			BlendPoses.Add(FPoseLink());
		}

		for(int32 Index=WeightNum; Index<Max; ++Index)
		{
			BlendWeights.Add(1.f);
		}

		for(int32 Index=LayerNum; Index<Max; ++Index)
		{
			LayerSetup.Add(FInputBlendPose());
		}
	}
}
#endif