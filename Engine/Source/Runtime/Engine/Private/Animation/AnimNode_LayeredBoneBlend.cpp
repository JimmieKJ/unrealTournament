// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_LayeredBoneBlend.h"
#include "AnimationRuntime.h"
#include "AnimTree.h"

/////////////////////////////////////////////////////
// FAnimNode_LayeredBoneBlend

void FAnimNode_LayeredBoneBlend::Initialize(const FAnimationInitializeContext& Context)
{
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
		check (Context.AnimInstance->CurrentSkeleton);
		ReinitializeBoneBlendWeights(Context.AnimInstance->RequiredBones, Context.AnimInstance->CurrentSkeleton);
	}
}

void FAnimNode_LayeredBoneBlend::ReinitializeBoneBlendWeights(const FBoneContainer& RequiredBones, const USkeleton* Skeleton)
{
	const int32 NumBones = RequiredBones.GetNumBones();
	FAnimationRuntime::CreateMaskWeights(NumBones, DesiredBoneBlendWeights, LayerSetup, RequiredBones, Skeleton);

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
}

void FAnimNode_LayeredBoneBlend::Update(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	// initialize children
	BasePose.Update(Context);

	const int NumPoses = BlendPoses.Num();
	if ( NumPoses > 0 )
	{
		for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
		{
			BlendPoses[ChildIndex].Update(Context);
		}

		if ( Context.AnimInstance->RequiredBones.GetNumBones() != DesiredBoneBlendWeights.Num() )
		{
			ReinitializeBoneBlendWeights(Context.AnimInstance->RequiredBones, Context.AnimInstance->CurrentSkeleton);
		}

		FAnimationRuntime::UpdateDesiredBoneWeight(DesiredBoneBlendWeights, CurrentBoneBlendWeights, BlendWeights, Context.AnimInstance->RequiredBones, Context.AnimInstance->CurrentSkeleton);
	}
}

void FAnimNode_LayeredBoneBlend::Evaluate(FPoseContext& Output)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNativeBlendPoses);

	const int NumPoses = BlendPoses.Num();
	if (NumPoses == 0)
	{
		BasePose.Evaluate(Output);
	}
	else
	{
		FPoseContext BasePoseContext(Output);

		// evaluate children
		BasePose.Evaluate(BasePoseContext);

		TArray<FA2Pose> TargetBlendPoses;
		for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
		{
			if (BlendWeights[ChildIndex] > ZERO_ANIMWEIGHT_THRESH)
			{
				FPoseContext CurrentPoseContext(Output);
				BlendPoses[ChildIndex].Evaluate(CurrentPoseContext);

				// @todo fixme : change this to not copy
				TargetBlendPoses.Add(CurrentPoseContext.Pose);
			}
			else
			{
				//Add something here so that array ordering is maintained
				FA2Pose Pose;
				FAnimationRuntime::FillWithRefPose(Pose.Bones, Output.AnimInstance->RequiredBones);
				TargetBlendPoses.Add(Pose);
			}
		}

		FAnimationRuntime::BlendPosesPerBoneFilter(BasePoseContext.Pose, TargetBlendPoses, Output.Pose, CurrentBoneBlendWeights, bMeshSpaceRotationBlend, Output.AnimInstance->RequiredBones, Output.AnimInstance->CurrentSkeleton);
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