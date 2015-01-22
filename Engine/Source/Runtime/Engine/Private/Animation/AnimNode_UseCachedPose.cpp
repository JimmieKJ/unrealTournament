// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_UseCachedPose.h"

/////////////////////////////////////////////////////
// FAnimNode_UseCachedPose

FAnimNode_UseCachedPose::FAnimNode_UseCachedPose()
{
}

void FAnimNode_UseCachedPose::Initialize(const FAnimationInitializeContext& Context)
{
	LinkToCachingNode.Initialize(Context);
}

void FAnimNode_UseCachedPose::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	LinkToCachingNode.CacheBones(Context);
}

void FAnimNode_UseCachedPose::Update(const FAnimationUpdateContext& Context)
{
	LinkToCachingNode.Update(Context);
}

void FAnimNode_UseCachedPose::Evaluate(FPoseContext& Output)
{
	LinkToCachingNode.Evaluate(Output);
}

void FAnimNode_UseCachedPose::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine, true);

	LinkToCachingNode.GatherDebugData(DebugData);
}