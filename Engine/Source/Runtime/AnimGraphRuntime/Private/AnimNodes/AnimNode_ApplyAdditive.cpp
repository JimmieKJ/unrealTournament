// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimNodes/AnimNode_ApplyAdditive.h"
#include "Animation/AnimInstanceProxy.h"

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

	ActualAlpha = 0.f;
	if (IsLODEnabled(Context.AnimInstanceProxy, LODThreshold))
	{
		// @note: If you derive from this class, and if you have input that you rely on for base
		// this is not going to work	
		EvaluateGraphExposedInputs.Execute(Context);
		ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);
		if (FAnimWeight::IsRelevant(ActualAlpha))
		{
			Additive.Update(Context.FractionalWeight(ActualAlpha));
		}
	}
}

void FAnimNode_ApplyAdditive::Evaluate(FPoseContext& Output)
{
	//@TODO: Could evaluate Base into Output and save a copy
	if (FAnimWeight::IsRelevant(ActualAlpha))
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

FAnimNode_ApplyAdditive::FAnimNode_ApplyAdditive()
	: Alpha(1.0f)
	, LODThreshold(INDEX_NONE)
	, ActualAlpha(0.f)
{
}

void FAnimNode_ApplyAdditive::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.1f%%)"), ActualAlpha*100.f);

	DebugData.AddDebugItem(DebugLine);
	Base.GatherDebugData(DebugData.BranchFlow(1.f));
	Additive.GatherDebugData(DebugData.BranchFlow(ActualAlpha));
}