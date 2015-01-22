// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/AnimNode_RotateRootBone.h"

/////////////////////////////////////////////////////
// FAnimNode_RotateRootBone

void FAnimNode_RotateRootBone::Initialize(const FAnimationInitializeContext& Context)
{
	BasePose.Initialize(Context);
}

void FAnimNode_RotateRootBone::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	BasePose.CacheBones(Context);
}

void FAnimNode_RotateRootBone::Update(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);
	BasePose.Update(Context);
}

void FAnimNode_RotateRootBone::Evaluate(FPoseContext& Output)
{
	// Evaluate the input
	BasePose.Evaluate(Output);

	checkSlow(!FMath::IsNaN(Yaw) && FMath::IsFinite(Yaw));
	checkSlow(!FMath::IsNaN(Pitch) && FMath::IsFinite(Pitch));

	// Build our desired rotation
	const FRotator DeltaRotation(Pitch, Yaw, 0.f);
	const FQuat DeltaQuat(DeltaRotation);
	const FQuat MeshToComponentQuat(MeshToComponent);

	// Convert our rotation from Component Space to Mesh Space.
	const FQuat MeshSpaceDeltaQuat = MeshToComponentQuat.Inverse() * DeltaQuat * MeshToComponentQuat;

	// Apply rotation to root bone.
	Output.Pose.Bones[0].SetRotation(Output.Pose.Bones[0].GetRotation() * MeshSpaceDeltaQuat);
}


void FAnimNode_RotateRootBone::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("(Pitch: %.2f Yaw: %.2f)"), Pitch, Yaw);
	DebugData.AddDebugItem(DebugLine);

	BasePose.GatherDebugData(DebugData);
}

FAnimNode_RotateRootBone::FAnimNode_RotateRootBone()
	: Pitch(0.0f)
	, Yaw(0.0f)
	, MeshToComponent(FRotator::ZeroRotator)
{
}
