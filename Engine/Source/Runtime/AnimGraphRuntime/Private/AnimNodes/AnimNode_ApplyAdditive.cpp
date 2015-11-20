// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimNodes/AnimNode_ApplyAdditive.h"

/////////////////////////////////////////////////////
// FAnimNode_ApplyAdditive

void FAnimNode_ApplyAdditive::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize(Context);

	Base.Initialize(Context);
	Additive.Initialize(Context);
}

void FAnimNode_ApplyAdditive::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	Base.CacheBones(Context);
	Additive.CacheBones(Context);
}

void FAnimNode_ApplyAdditive::Update(const FAnimationUpdateContext& Context)
{
	Base.Update(Context);

	if (IsLODEnabled(Context.AnimInstance, LODThreshold))
	{
		// @note: If you derive from this class, and if you have input that you rely on for base
		// this is not going to work	
		EvaluateGraphExposedInputs.Execute(Context);
		const float ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);
		if (ActualAlpha > ZERO_ANIMWEIGHT_THRESH)
		{
			Additive.Update(Context.FractionalWeight(ActualAlpha));
		}
	}
}

void FAnimNode_ApplyAdditive::Evaluate(FPoseContext& Output)
{
	if (IsLODEnabled(Output.AnimInstance, LODThreshold))
	{
		//@TODO: Could evaluate Base into Output and save a copy
		const float ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);
		if (ActualAlpha > ZERO_ANIMWEIGHT_THRESH)
		{
			FPoseContext AdditiveEvalContext(Output);

			Base.Evaluate(Output);
			Additive.Evaluate(AdditiveEvalContext);

			FAnimationRuntime::AccumulateAdditivePose(Output.Pose, AdditiveEvalContext.Pose, Output.Curve, AdditiveEvalContext.Curve, ActualAlpha, AAT_LocalSpaceBase);
			Output.Pose.NormalizeRotations();
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

FAnimNode_ApplyAdditive::FAnimNode_ApplyAdditive()
	: Alpha(1.0f)
	, LODThreshold(INDEX_NONE)
{
}

void FAnimNode_ApplyAdditive::GatherDebugData(FNodeDebugData& DebugData)
{
	const float ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);

	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.1f%%)"), ActualAlpha*100.f);

	DebugData.AddDebugItem(DebugLine);
	Base.GatherDebugData(DebugData.BranchFlow(1.f));
	Additive.GatherDebugData(DebugData.BranchFlow(ActualAlpha));
}