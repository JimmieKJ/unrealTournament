// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_BlendSpacePlayer.h"
#include "Animation/BlendSpaceBase.h"

/////////////////////////////////////////////////////
// FAnimNode_BlendSpacePlayer

FAnimNode_BlendSpacePlayer::FAnimNode_BlendSpacePlayer()
	: X(0.0f)
	, Y(0.0f)
	, Z(0.0f)
	, PlayRate(1.0f)
	, bLoop(true)
	, BlendSpace(NULL)
	, GroupIndex(INDEX_NONE)
	, InternalTimeAccumulator(0.0f)
{
}

void FAnimNode_BlendSpacePlayer::Initialize(const FAnimationInitializeContext& Context)
{
	BlendSampleDataCache.Empty();
	
	EvaluateGraphExposedInputs.Execute(Context);
	if(PlayRate >= 0.0f)
	{
		InternalTimeAccumulator = 0.0f;
	}
	else
	{
		// Blend spaces run between 0 and 1
		InternalTimeAccumulator = 1.0f;
	}

	if (BlendSpace != NULL)
	{
		BlendSpace->InitializeFilter(&BlendFilter);
	}
}

void FAnimNode_BlendSpacePlayer::CacheBones(const FAnimationCacheBonesContext& Context) 
{
}

void FAnimNode_BlendSpacePlayer::Update(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	UpdateInternal(Context);
}

void FAnimNode_BlendSpacePlayer::UpdateInternal(const FAnimationUpdateContext& Context)
{
	if ((BlendSpace != NULL) && (Context.AnimInstance->CurrentSkeleton->IsCompatible(BlendSpace->GetSkeleton())))
	{
		// Create a tick record and fill it out
		FAnimGroupInstance* SyncGroup;
		FAnimTickRecord& TickRecord = Context.AnimInstance->CreateUninitializedTickRecord(GroupIndex, /*out*/ SyncGroup);

		const FVector BlendInput(X, Y, Z);
	
		Context.AnimInstance->MakeBlendSpaceTickRecord(TickRecord, BlendSpace, BlendInput, BlendSampleDataCache, BlendFilter, bLoop, PlayRate, Context.GetFinalBlendWeight(), /*inout*/ InternalTimeAccumulator);

		// Update the sync group if it exists
		if (SyncGroup != NULL)
		{
			SyncGroup->TestTickRecordForLeadership(GroupRole);
		}
	}
}

void FAnimNode_BlendSpacePlayer::Evaluate(FPoseContext& Output)
{
	if ((BlendSpace != NULL) && (Output.AnimInstance->CurrentSkeleton->IsCompatible(BlendSpace->GetSkeleton())))
	{
		Output.AnimInstance->BlendSpaceEvaluatePose(BlendSpace, BlendSampleDataCache, Output.Pose);
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_BlendSpacePlayer::OverrideAsset(UAnimationAsset* NewAsset)
{
	if(UBlendSpaceBase* NewBlendSpace = Cast<UBlendSpaceBase>(NewAsset))
	{
		BlendSpace = NewBlendSpace;
	}
}

void FAnimNode_BlendSpacePlayer::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	if (BlendSpace)
	{
		DebugLine += FString::Printf(TEXT("('%s' Play Time: %.3f)"), *BlendSpace->GetName(), InternalTimeAccumulator);

		DebugData.AddDebugItem(DebugLine, true);

		//BlendSpace->GetBlendSamples();
	}
}