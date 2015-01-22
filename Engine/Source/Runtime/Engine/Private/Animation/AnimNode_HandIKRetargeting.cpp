// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/BoneControllers/AnimNode_HandIKRetargeting.h"

/////////////////////////////////////////////////////
// FAnimNode_HandIKRetargeting

FAnimNode_HandIKRetargeting::FAnimNode_HandIKRetargeting()
: HandFKWeight(0.5f)
{
}

void FAnimNode_HandIKRetargeting::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += FString::Printf(TEXT(" HandFKWeight: %f)"), HandFKWeight);
	for (int32 BoneIndex = 0; BoneIndex < IKBonesToMove.Num(); BoneIndex++)
	{
		DebugLine += FString::Printf(TEXT(", %s)"), *IKBonesToMove[BoneIndex].BoneName.ToString());
	}
	DebugLine += FString::Printf(TEXT(")"));
	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_HandIKRetargeting::EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, const FBoneContainer& RequiredBones, FA2CSPose& MeshBases, TArray<FBoneTransform>& OutBoneTransforms)
{
	checkSlow(OutBoneTransforms.Num() == 0);

	// Get component space transforms for all of our IK and FK bones.
	FTransform const RightHandFKTM = MeshBases.GetComponentSpaceTransform(RightHandFK.BoneIndex);
	FTransform const LeftHandFKTM = MeshBases.GetComponentSpaceTransform(LeftHandFK.BoneIndex);
	FTransform const RightHandIKTM = MeshBases.GetComponentSpaceTransform(RightHandIK.BoneIndex);
	FTransform const LeftHandIKTM = MeshBases.GetComponentSpaceTransform(LeftHandIK.BoneIndex);
	
	// Compute weight FK and IK hand location. And translation from IK to FK.
	FVector const FKLocation = FMath::Lerp<FVector>(LeftHandFKTM.GetTranslation(), RightHandFKTM.GetTranslation(), HandFKWeight);
	FVector const IKLocation = FMath::Lerp<FVector>(LeftHandIKTM.GetTranslation(), RightHandIKTM.GetTranslation(), HandFKWeight);
	FVector const IK_To_FK_Translation = FKLocation - IKLocation;

	// If we're not translating, don't send any bones to update.
	if (!IK_To_FK_Translation.IsNearlyZero())
	{
		// Move desired bones
		for (int32 BoneIndex = 0; BoneIndex < IKBonesToMove.Num(); BoneIndex++)
		{
			FBoneReference const & BoneReference = IKBonesToMove[BoneIndex];
			if (BoneReference.IsValid(RequiredBones))
			{
				FTransform BoneTransform = MeshBases.GetComponentSpaceTransform(BoneReference.BoneIndex);
				BoneTransform.AddToTranslation(IK_To_FK_Translation);

				OutBoneTransforms.Add(FBoneTransform(BoneReference.BoneIndex, BoneTransform));
			}
		}
	}
}

bool FAnimNode_HandIKRetargeting::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	if (RightHandFK.IsValid(RequiredBones)
		&& LeftHandFK.IsValid(RequiredBones)
		&& RightHandIK.IsValid(RequiredBones)
		&& LeftHandIK.IsValid(RequiredBones))
	{
		// we need at least one bone to move valid.
		for (int32 BoneIndex = 0; BoneIndex < IKBonesToMove.Num(); BoneIndex++)
		{
			if (IKBonesToMove[BoneIndex].IsValid(RequiredBones))
			{
				return true;
			}
		}
	}

	return false;
}

void FAnimNode_HandIKRetargeting::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	RightHandFK.Initialize(RequiredBones);
	LeftHandFK.Initialize(RequiredBones);
	RightHandIK.Initialize(RequiredBones);
	LeftHandIK.Initialize(RequiredBones);

	for (int32 BoneIndex = 0; BoneIndex < IKBonesToMove.Num(); BoneIndex++)
	{
		IKBonesToMove[BoneIndex].Initialize(RequiredBones);
	}
}