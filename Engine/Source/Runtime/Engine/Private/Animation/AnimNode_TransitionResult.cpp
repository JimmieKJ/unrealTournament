// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_TransitionResult.h"

/////////////////////////////////////////////////////
// FAnimNode_TransitionResult

FAnimNode_TransitionResult::FAnimNode_TransitionResult()
{
}

void FAnimNode_TransitionResult::Initialize(const FAnimationInitializeContext& Context)
{
}

void FAnimNode_TransitionResult::CacheBones(const FAnimationCacheBonesContext& Context) 
{
}

void FAnimNode_TransitionResult::Update(const FAnimationUpdateContext& Context)
{
}

void FAnimNode_TransitionResult::Evaluate(FPoseContext& Output)
{
}


void FAnimNode_TransitionResult::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);
}