// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimNodes/AnimNode_SaveCachedPose.h"

/////////////////////////////////////////////////////
// FAnimNode_SaveCachedPose

FAnimNode_SaveCachedPose::FAnimNode_SaveCachedPose()
{
}

void FAnimNode_SaveCachedPose::Initialize(const FAnimationInitializeContext& Context)
{
	// StateMachines cause reinitialization on state changes.
	// we only want to let them through if we're not relevant as to not create a pop.
	if (!InitializationCounter.IsSynchronizedWith(Context.AnimInstance->InitializationCounter)
		|| ((EvaluationCounter.Get() != INDEX_NONE) && !EvaluationCounter.WasSynchronizedInTheLastFrame(Context.AnimInstance->EvaluationCounter)))
	{
		InitializationCounter.SynchronizeWith(Context.AnimInstance->InitializationCounter);

		// Reset update counter here as Initialize can happen as part of a state machine update
		// when the current state changes. This ensures that any subsequent updates that may 
		// use this cached pose get properly updated.
		UpdateCounter.Reset();

		FAnimNode_Base::Initialize(Context);

		// Initialize the subgraph
		Pose.Initialize(Context);
	}
}

void FAnimNode_SaveCachedPose::CacheBones(const FAnimationCacheBonesContext& Context)
{
	if (!CachedBonesCounter.IsSynchronizedWith(Context.AnimInstance->CachedBonesCounter))
	{
		CachedBonesCounter.SynchronizeWith(Context.AnimInstance->CachedBonesCounter);

		// Cache bones in the subgraph
		Pose.CacheBones(Context);
	}
}

void FAnimNode_SaveCachedPose::Update(const FAnimationUpdateContext& Context)
{
	if (!UpdateCounter.IsSynchronizedWith(Context.AnimInstance->UpdateCounter))
	{
		UpdateCounter.SynchronizeWith(Context.AnimInstance->UpdateCounter);

		// Update the subgraph
		Pose.Update(Context);
	}
}

void FAnimNode_SaveCachedPose::Evaluate(FPoseContext& Output)
{
	if (!EvaluationCounter.IsSynchronizedWith(Output.AnimInstance->EvaluationCounter))
	{
		EvaluationCounter.SynchronizeWith(Output.AnimInstance->EvaluationCounter);

		FPoseContext CachingContext(Output);
		Pose.Evaluate(CachingContext);
		CachedPose.MoveBonesFrom(CachingContext.Pose);
		CachedCurve.MoveFrom(CachingContext.Curve);
	}

	// Return the cached result
	Output.Pose.CopyBonesFrom(CachedPose);
	Output.Curve.CopyFrom(CachedCurve);
}


void FAnimNode_SaveCachedPose::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);

	if (!DebugDataCounter.IsSynchronizedWith(DebugData.AnimInstance->DebugDataCounter))
	{
		DebugDataCounter.SynchronizeWith(DebugData.AnimInstance->DebugDataCounter);
		Pose.GatherDebugData(DebugData);
	}
}