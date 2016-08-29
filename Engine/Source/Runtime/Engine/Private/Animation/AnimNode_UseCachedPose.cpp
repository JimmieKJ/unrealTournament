// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_UseCachedPose.h"

/////////////////////////////////////////////////////
// FAnimNode_UseCachedPose

FAnimNode_UseCachedPose::FAnimNode_UseCachedPose()
{
}

void FAnimNode_UseCachedPose::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize(Context);

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
	DebugLine += FString::Printf(TEXT("%s:"), *CachePoseName.ToString());

	DebugData.AddDebugItem(DebugLine, true);

	// we explicitly do not forward this call to the SaveCachePose node here.
	// It is handled in FAnimInstanceProxy::GatherDebugData
}