// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_SequenceEvaluator.h"

/////////////////////////////////////////////////////
// FAnimSequenceEvaluatorNode

void FAnimNode_SequenceEvaluator::Initialize(const FAnimationInitializeContext& Context)
{
}

void FAnimNode_SequenceEvaluator::CacheBones(const FAnimationCacheBonesContext& Context) 
{
}

void FAnimNode_SequenceEvaluator::Update(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);
}

void FAnimNode_SequenceEvaluator::Evaluate(FPoseContext& Output)
{
	if ((Sequence != NULL) && (Output.AnimInstance->CurrentSkeleton->IsCompatible(Sequence->GetSkeleton())))
	{
		Output.AnimInstance->SequenceEvaluatePose(Sequence, Output.Pose, FAnimExtractContext(ExplicitTime));
	}
	else
	{
		Output.ResetToRefPose();
	}
}
void FAnimNode_SequenceEvaluator::OverrideAsset(UAnimationAsset* NewAsset)
{
	if(UAnimSequenceBase* NewSequence = Cast<UAnimSequenceBase>(NewAsset))
	{
		Sequence = NewSequence;
	}
}

void FAnimNode_SequenceEvaluator::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("('%s' Play Time: %.3f)"), *Sequence->GetName(), ExplicitTime);
	DebugData.AddDebugItem(DebugLine, true);
}