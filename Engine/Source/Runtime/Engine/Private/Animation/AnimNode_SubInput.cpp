// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_SubInput.h"

void FAnimNode_SubInput::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize(Context);
}

void FAnimNode_SubInput::CacheBones(const FAnimationCacheBonesContext& Context)
{
}

void FAnimNode_SubInput::Update(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);
}

void FAnimNode_SubInput::Evaluate(FPoseContext& Output)
{
	if(InputPose.IsValid() && InputCurve.IsValid())
	{
		Output.Pose.CopyBonesFrom(InputPose);
		Output.Curve.CopyFrom(InputCurve);
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_SubInput::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);
}
