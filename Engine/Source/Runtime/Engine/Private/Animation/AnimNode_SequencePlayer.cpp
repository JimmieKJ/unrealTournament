// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimSequencePlayerNode

float FAnimNode_SequencePlayer::GetCurrentAssetTime()
{
	return InternalTimeAccumulator;
}

float FAnimNode_SequencePlayer::GetCurrentAssetLength()
{
	return Sequence ? Sequence->SequenceLength : 0.0f;
}

void FAnimNode_SequencePlayer::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_AssetPlayerBase::Initialize(Context);

	EvaluateGraphExposedInputs.Execute(Context);
	InternalTimeAccumulator = StartPosition;
	if (Sequence != NULL)
	{
		InternalTimeAccumulator = FMath::Clamp(StartPosition, 0.f, Sequence->SequenceLength);

		if (StartPosition == 0.f && (PlayRate * Sequence->RateScale) < 0.0f)
		{
			InternalTimeAccumulator = Sequence->SequenceLength;
		}
	}
}

void FAnimNode_SequencePlayer::CacheBones(const FAnimationCacheBonesContext& Context) 
{
}

void FAnimNode_SequencePlayer::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	if ((Sequence != NULL) && (Context.AnimInstanceProxy->IsSkeletonCompatible(Sequence->GetSkeleton())))
	{
		InternalTimeAccumulator = FMath::Clamp(InternalTimeAccumulator, 0.f, Sequence->SequenceLength);
		CreateTickRecordForNode(Context, Sequence, bLoopAnimation, PlayRate);
	}
}

void FAnimNode_SequencePlayer::Evaluate(FPoseContext& Output)
{
	if ((Sequence != NULL) && (Output.AnimInstanceProxy->IsSkeletonCompatible(Sequence->GetSkeleton())))
	{
		Sequence->GetAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext(InternalTimeAccumulator, Output.AnimInstanceProxy->ShouldExtractRootMotion()));
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_SequencePlayer::OverrideAsset(UAnimationAsset* NewAsset)
{
	if(UAnimSequenceBase* AnimSequence = Cast<UAnimSequenceBase>(NewAsset))
	{
		Sequence = AnimSequence;
	}
}

void FAnimNode_SequencePlayer::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("('%s' Play Time: %.3f)"), *Sequence->GetName(), InternalTimeAccumulator);
	DebugData.AddDebugItem(DebugLine, true);
}

float FAnimNode_SequencePlayer::GetTimeFromEnd(float CurrentNodeTime)
{
	return Sequence->GetMaxCurrentTime() - CurrentNodeTime;
}
