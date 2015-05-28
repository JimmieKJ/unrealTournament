// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_BlendListBase.h"
#include "AnimationRuntime.h"
#include "AnimTree.h"

/////////////////////////////////////////////////////
// FAnimNode_BlendListBase

void FAnimNode_BlendListBase::Initialize(const FAnimationInitializeContext& Context)
{
	const int NumPoses = BlendPose.Num();
	checkSlow(BlendTime.Num() == NumPoses);

	BlendWeights.Empty(NumPoses);
	if (NumPoses > 0)
	{
		BlendWeights.AddZeroed(NumPoses);
		BlendWeights[0] = 1.0f;

		for (int32 ChildIndex = 0; ChildIndex < NumPoses; ++ChildIndex)
		{
			BlendPose[ChildIndex].Initialize(Context);
		}
	}

	RemainingBlendTimes.Empty(NumPoses);
	RemainingBlendTimes.AddZeroed(NumPoses);

	LastActiveChildIndex = INDEX_NONE;
}

void FAnimNode_BlendListBase::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	for(int32 ChildIndex=0; ChildIndex<BlendPose.Num(); ChildIndex++)
	{
		BlendPose[ChildIndex].CacheBones(Context);
	}
}

void FAnimNode_BlendListBase::Update(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	const int NumPoses = BlendPose.Num();
	checkSlow((BlendTime.Num() == NumPoses) && (BlendWeights.Num() == NumPoses));

	if (NumPoses > 0)
	{
		// Handle a change in the active child index; adjusting the target weights
		const int32 ChildIndex = GetActiveChildIndex();

		if (ChildIndex != LastActiveChildIndex)
		{
			bool LastChildIndexIsInvalid = (LastActiveChildIndex == INDEX_NONE);
			LastActiveChildIndex = ChildIndex;

			const float CurrentWeight = BlendWeights[ChildIndex];
			const float DesiredWeight = 1.0f;
			const float WeightDifference = FMath::Clamp<float>(FMath::Abs<float>(DesiredWeight - CurrentWeight), 0.0f, 1.0f);

			// scale by the weight difference since we want always consistency:
			// - if you're moving from 0 to full weight 1, it will use the normal blend time
			// - if you're moving from 0.5 to full weight 1, it will get there in half the time
			const float RemainingBlendTime = LastChildIndexIsInvalid ? 0.0f : ( BlendTime[ChildIndex] * WeightDifference );

			for (int32 i = 0; i < RemainingBlendTimes.Num(); ++i)
			{
				RemainingBlendTimes[i] = RemainingBlendTime;
			}
		}

		// Advance the weights
		//@TODO: This means we advance even in a frame where the target weights/times just got modified; is that desirable?
		float SumWeight = 0.0f;
		for (int32 i = 0; i < RemainingBlendTimes.Num(); ++i)
		{
			float& RemainingBlendTime = RemainingBlendTimes[i];
			float& BlendWeight = BlendWeights[i];

			const float DesiredWeight = (i == ChildIndex) ? 1.0f : 0.0f;

			FAnimationRuntime::TickBlendWeight(Context.GetDeltaTime(), DesiredWeight, BlendWeight, RemainingBlendTime);

			SumWeight += BlendWeight;
		}

		// Renormalize the weights
		if ((SumWeight > ZERO_ANIMWEIGHT_THRESH) && (FMath::Abs<float>(SumWeight - 1.0f) > ZERO_ANIMWEIGHT_THRESH))
		{
			float ReciprocalSum = 1.0f / SumWeight;
			for (int32 i = 0; i < BlendWeights.Num(); ++i)
			{
				BlendWeights[i] *= ReciprocalSum;
			}
		}

		// Update our active children
		for (int32 i = 0; i < BlendPose.Num(); ++i)
		{
			const float BlendWeight = BlendWeights[i];
			if (BlendWeight > ZERO_ANIMWEIGHT_THRESH)
			{
				BlendPose[i].Update(Context.FractionalWeight(BlendWeight));
			}
		}
	}
}

void FAnimNode_BlendListBase::Evaluate(FPoseContext& Output)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNativeBlendPoses);

	const int32 MaxNumPoses = BlendPose.Num();

	//@TODO: This is currently using O(NumPoses) memory but doesn't need to
	if ((MaxNumPoses > 0) && (BlendPose.Num() == BlendWeights.Num()))
	{
		TArray<FPoseContext> FilteredPoseContexts;
		FilteredPoseContexts.Empty(MaxNumPoses);

		FTransformArrayA2** FilteredPoses = new FTransformArrayA2*[MaxNumPoses];
		float* FilteredWeights = new float[MaxNumPoses];

		int32 NumActivePoses = 0;
		for (int32 i = 0; i < BlendPose.Num(); ++i)
		{
			const float BlendWeight = BlendWeights[i];
			if (BlendWeight > ZERO_ANIMWEIGHT_THRESH)
			{
				FPoseContext& CurrentPoseContext = *(new (FilteredPoseContexts) FPoseContext(Output));

				FPoseLink& CurrentPose = BlendPose[i];
				CurrentPose.Evaluate(CurrentPoseContext);

				FilteredPoses[NumActivePoses] = &(CurrentPoseContext.Pose.Bones);
				FilteredWeights[NumActivePoses] = BlendWeight;
				NumActivePoses++;
			}
		}

		FAnimationRuntime::BlendPosesTogether(NumActivePoses, (const FTransformArrayA2**)FilteredPoses, (const float*)FilteredWeights, Output.AnimInstance->RequiredBones, Output.Pose.Bones);

		delete[] FilteredPoses;
		delete[] FilteredWeights;
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_BlendListBase::GatherDebugData(FNodeDebugData& DebugData)
{
	const int NumPoses = BlendPose.Num();
	const int32 ChildIndex = GetActiveChildIndex();

	FString DebugLine = GetNodeName(DebugData);
	DebugLine += FString::Printf(TEXT("(Active: (%i/%i) BlendWeight: %.1f%% BlendTime %.3f)"), ChildIndex+1, NumPoses, BlendWeights[ChildIndex]*100.f, BlendTime[ChildIndex]);

	DebugData.AddDebugItem(DebugLine);
	
	for(int32 Pose = 0; Pose < NumPoses; ++Pose)
	{
		BlendPose[Pose].GatherDebugData(DebugData.BranchFlow(BlendWeights[Pose]));
	}
}