// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"

/////////////////////////////////////////////////////
// FAnimationNode_TwoWayBlend

void FAnimationNode_TwoWayBlend::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize(Context);

	A.Initialize(Context);
	B.Initialize(Context);
}

void FAnimationNode_TwoWayBlend::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	A.CacheBones(Context);
	B.CacheBones(Context);
}

void FAnimationNode_TwoWayBlend::Update(const FAnimationUpdateContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAnimationNode_TwoWayBlend_Update);
	EvaluateGraphExposedInputs.Execute(Context);

	const float ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);
	if (ActualAlpha > ZERO_ANIMWEIGHT_THRESH)
	{
		if (ActualAlpha < 1.0f - ZERO_ANIMWEIGHT_THRESH)
		{
			// Blend A and B together
			A.Update(Context.FractionalWeight(1.0f - ActualAlpha));
			B.Update(Context.FractionalWeight(ActualAlpha));
		}
		else
		{
			// Take all of B
			B.Update(Context);
		}
	}
	else
	{
		// Take all of A
		A.Update(Context);
	}
}

void FAnimationNode_TwoWayBlend::Evaluate(FPoseContext& Output)
{
	const float ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);
	if (ActualAlpha > ZERO_ANIMWEIGHT_THRESH)
	{
		if (ActualAlpha < 1.0f - ZERO_ANIMWEIGHT_THRESH)
		{
			FPoseContext Pose1(Output);
			FPoseContext Pose2(Output);

			A.Evaluate(Pose1);
			B.Evaluate(Pose2);

			FAnimationRuntime::BlendTwoPosesTogether(Pose1.Pose, Pose2.Pose, Pose1.Curve, Pose2.Curve, (1.0f - ActualAlpha), Output.Pose, Output.Curve);
		}
		else
		{
			B.Evaluate(Output);
		}
	}
	else
	{
		A.Evaluate(Output);
	}
}


void FAnimationNode_TwoWayBlend::GatherDebugData(FNodeDebugData& DebugData)
{
	const float ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);

	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.1f%%)"), ActualAlpha*100);
	DebugData.AddDebugItem(DebugLine);

	A.GatherDebugData(DebugData.BranchFlow(1.f - ActualAlpha));
	B.GatherDebugData(DebugData.BranchFlow(ActualAlpha));
}