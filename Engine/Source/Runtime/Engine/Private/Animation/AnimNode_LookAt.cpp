// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/BoneControllers/AnimNode_LookAt.h"
#include "AnimationRuntime.h"

/////////////////////////////////////////////////////
// FAnimNode_LookAt

FAnimNode_LookAt::FAnimNode_LookAt()
	: LookAtLocation(FVector::ZeroVector)
	, LookAtAxis(EAxis::X)
	, CurrentLookAtLocation(FVector::ZeroVector)
{
}

void FAnimNode_LookAt::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	if (LookAtBone.BoneIndex != INDEX_NONE)
	{
		DebugLine += FString::Printf(TEXT(" Target: %s, Look At Bone: %s, Location : %s)"), *BoneToModify.BoneName.ToString(), *LookAtBone.BoneName.ToString(), *CurrentLookAtLocation.ToString());
	}
	else
	{
		DebugLine += FString::Printf(TEXT(" Target: %s, Look At Location : %s, Location : %s)"), *BoneToModify.BoneName.ToString(), *LookAtLocation.ToString(), *CurrentLookAtLocation.ToString());
	}

	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_LookAt::EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, const FBoneContainer& RequiredBones, FA2CSPose& MeshBases, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	FTransform ComponentBoneTransform = MeshBases.GetComponentSpaceTransform(BoneToModify.BoneIndex);

	// get target location
	FVector TargetLocationInComponentSpace;
	if (LookAtBone.IsValid(RequiredBones))
	{
		FTransform LookAtTransform  = MeshBases.GetComponentSpaceTransform(LookAtBone.BoneIndex);
		TargetLocationInComponentSpace = LookAtTransform.GetLocation();
	}
	else
	{
		TargetLocationInComponentSpace = SkelComp->ComponentToWorld.InverseTransformPosition(LookAtLocation);
	}
	
	CurrentLookAtLocation = TargetLocationInComponentSpace;

	// lookat vector
	FVector LookAtVector = GetAlignVector(ComponentBoneTransform, LookAtAxis);
	// flip to target vector if it wasnt negative
	bool bShouldFlip = LookAtAxis == EAxisOption::X_Neg || LookAtAxis == EAxisOption::Y_Neg || LookAtAxis == EAxisOption::Z_Neg;
	FVector ToTarget = CurrentLookAtLocation - ComponentBoneTransform.GetLocation();
	ToTarget.Normalize();
	if (bShouldFlip)
	{
		ToTarget *= -1.f;
	}
	
	// get delta rotation
	FQuat DeltaRot = FQuat::FindBetween(LookAtVector, ToTarget);

	// transform current rotation to delta rotation
	FQuat CurrentRot = ComponentBoneTransform.GetRotation();
	FQuat NewRotation = DeltaRot * CurrentRot;
	ComponentBoneTransform.SetRotation(NewRotation);

	OutBoneTransforms.Add( FBoneTransform(BoneToModify.BoneIndex, ComponentBoneTransform) );
}

bool FAnimNode_LookAt::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) 
{
	// if both bones are valid
	return (BoneToModify.IsValid(RequiredBones) && 
		// or if name isn't set (use Look At Location) or Look at bone is valid 
		// do not call isValid since that means if look at bone isn't in LOD, we won't evaluate
		// we still should evaluate as long as the BoneToModify is valid even LookAtBone isn't included in required bones
		(LookAtBone.BoneName == NAME_None || LookAtBone.BoneIndex != INDEX_NONE) );
}

void FAnimNode_LookAt::InitializeBoneReferences(const FBoneContainer& RequiredBones) 
{
	BoneToModify.Initialize(RequiredBones);
	LookAtBone.Initialize(RequiredBones);
}

FVector FAnimNode_LookAt::GetAlignVector(FTransform& Transform, EAxisOption::Type AxisOption)
{
	switch (AxisOption)
	{
	case EAxisOption::X:
	case EAxisOption::X_Neg:
		return Transform.GetUnitAxis(EAxis::X);
	case EAxisOption::Y:
	case EAxisOption::Y_Neg:
		return Transform.GetUnitAxis(EAxis::Y);
	case EAxisOption::Z:
	case EAxisOption::Z_Neg:
		return Transform.GetUnitAxis(EAxis::Z);
	}

	return FVector(1.f, 0.f, 0.f);
}