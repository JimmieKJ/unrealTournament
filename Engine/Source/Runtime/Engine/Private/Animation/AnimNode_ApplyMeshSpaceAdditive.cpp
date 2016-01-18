// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_ApplyMeshSpaceAdditive.h"

/////////////////////////////////////////////////////
// FAnimNode_ApplyMeshSpaceAdditive

void FAnimNode_ApplyMeshSpaceAdditive::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize(Context);

	Base.Initialize(Context);
	Additive.Initialize(Context);
}

void FAnimNode_ApplyMeshSpaceAdditive::CacheBones(const FAnimationCacheBonesContext& Context)
{
	Base.CacheBones(Context);
	Additive.CacheBones(Context);
}

void FAnimNode_ApplyMeshSpaceAdditive::Update(const FAnimationUpdateContext& Context)
{
	Base.Update(Context);

	if (IsLODEnabled(Context.AnimInstanceProxy, LODThreshold))
	{
		// @note: If you derive this class, and if you have input that you rely on for base
		// this is not going to work	
		EvaluateGraphExposedInputs.Execute(Context);
		const float ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);
		if (ActualAlpha > ZERO_ANIMWEIGHT_THRESH)
		{
			Additive.Update(Context.FractionalWeight(ActualAlpha));
		}
	}
}

void FAnimNode_ApplyMeshSpaceAdditive::Evaluate(FPoseContext& Output)
{
	if (IsLODEnabled(Output.AnimInstanceProxy, LODThreshold))
	{
		//@TODO: Could evaluate Base into Output and save a copy
		const float ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);
		if (ActualAlpha > ZERO_ANIMWEIGHT_THRESH)
		{
			FPoseContext AdditiveEvalContext(Output);

			Base.Evaluate(Output);
			Additive.Evaluate(AdditiveEvalContext);

			FAnimationRuntime::AccumulateMeshSpaceRotationAdditiveToLocalPose(Output.Pose, AdditiveEvalContext.Pose, Output.Curve, AdditiveEvalContext.Curve, ActualAlpha);

		}
		else
		{
			Base.Evaluate(Output);
		}
	}
	else
	{
		Base.Evaluate(Output);
	}
}

FAnimNode_ApplyMeshSpaceAdditive::FAnimNode_ApplyMeshSpaceAdditive()
	: Alpha(1.0f)
	, LODThreshold(INDEX_NONE)
{
}

void FAnimNode_ApplyMeshSpaceAdditive::GatherDebugData(FNodeDebugData& DebugData)
{
	const float ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);

	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.1f%%)"), ActualAlpha*100.f);

	DebugData.AddDebugItem(DebugLine);
	Base.GatherDebugData(DebugData.BranchFlow(1.f));
	Additive.GatherDebugData(DebugData.BranchFlow(ActualAlpha));
}