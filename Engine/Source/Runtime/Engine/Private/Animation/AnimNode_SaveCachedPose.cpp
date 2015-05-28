// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_SaveCachedPose.h"

/////////////////////////////////////////////////////
// FAnimNode_SaveCachedPose

FAnimNode_SaveCachedPose::FAnimNode_SaveCachedPose()
	: LastInitializedContextCounter(INDEX_NONE)
	, LastCacheBonesContextCounter(INDEX_NONE)
	, LastUpdatedContextCounter(INDEX_NONE)
	, LastEvaluatedContextCounter(INDEX_NONE)
{
}

void FAnimNode_SaveCachedPose::Initialize(const FAnimationInitializeContext& Context)
{
	if (LastInitializedContextCounter != Context.AnimInstance->GetGraphTraversalCounter())
	{
		LastInitializedContextCounter = Context.AnimInstance->GetGraphTraversalCounter();

		// Initialize the subgraph
		Pose.Initialize(Context);
	}
}

void FAnimNode_SaveCachedPose::CacheBones(const FAnimationCacheBonesContext& Context)
{
	if (LastCacheBonesContextCounter != Context.AnimInstance->GetGraphTraversalCounter())
	{
		LastCacheBonesContextCounter = Context.AnimInstance->GetGraphTraversalCounter();

		// Initialize the subgraph
		Pose.CacheBones(Context);
	}
}

void FAnimNode_SaveCachedPose::Update(const FAnimationUpdateContext& Context)
{
	if (LastUpdatedContextCounter != Context.AnimInstance->GetGraphTraversalCounter())
	{
		LastUpdatedContextCounter = Context.AnimInstance->GetGraphTraversalCounter();

		// Update the subgraph
		Pose.Update(Context);
	}
}

void FAnimNode_SaveCachedPose::Evaluate(FPoseContext& Output)
{
	if (LastEvaluatedContextCounter != Output.AnimInstance->GetGraphTraversalCounter())
	{
		LastEvaluatedContextCounter = Output.AnimInstance->GetGraphTraversalCounter();

		FPoseContext CachingContext(Output);
		Pose.Evaluate(CachingContext);
		CachedPose = CachingContext.Pose;
	}

	// Return the cached result
	Output.AnimInstance->CopyPose(CachedPose, Output.Pose);
}


void FAnimNode_SaveCachedPose::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);

	Pose.GatherDebugData(DebugData);
}