// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "Animation/AnimTypes.h"
#include "AnimNode_CopyBoneDelta.h"

FAnimNode_CopyBoneDelta::FAnimNode_CopyBoneDelta()
	: bCopyTranslation(false)
	, bCopyRotation(false)
	, bCopyScale(false)
	, CopyMode(CopyBoneDeltaMode::Accumulate)
	, TranslationMultiplier(1.0f)
	, RotationMultiplier(1.0f)
	, ScaleMultiplier(1.0f)
{

}

void FAnimNode_CopyBoneDelta::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_CopyBoneDelta::EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms)
{
	if(!bCopyTranslation && !bCopyRotation && !bCopyScale)
	{
		return;
	}

	const FBoneContainer& BoneContainer = MeshBases.GetPose().GetBoneContainer();
	FCompactPoseBoneIndex SourceBoneIndex = SourceBone.GetCompactPoseIndex(BoneContainer);
	FCompactPoseBoneIndex TargetBoneIndex = TargetBone.GetCompactPoseIndex(BoneContainer);

	FTransform SourceTM = MeshBases.GetComponentSpaceTransform(SourceBoneIndex);
	FTransform TargetTM = MeshBases.GetComponentSpaceTransform(TargetBoneIndex);

	// Convert to parent space
	FAnimationRuntime::ConvertCSTransformToBoneSpace(SkelComp, MeshBases, SourceTM, SourceBoneIndex, BCS_ParentBoneSpace);
	FAnimationRuntime::ConvertCSTransformToBoneSpace(SkelComp, MeshBases, TargetTM, TargetBoneIndex, BCS_ParentBoneSpace);

	// Ref pose transform
	FTransform RefLSTransform = SkelComp->SkeletalMesh->RefSkeleton.GetRefBonePose()[SourceBone.GetMeshPoseIndex().GetInt()];

	// Get transform relative to ref pose
	SourceTM.SetToRelativeTransform(RefLSTransform);

	if(CopyMode == CopyBoneDeltaMode::Accumulate)
	{
		if(bCopyTranslation)
		{
			TargetTM.AddToTranslation(SourceTM.GetTranslation() * TranslationMultiplier);
		}
		if(bCopyRotation)
		{
			FVector Axis;
			float Angle;
			SourceTM.GetRotation().ToAxisAndAngle(Axis, Angle);

			TargetTM.SetRotation(FQuat(Axis, Angle * RotationMultiplier) * TargetTM.GetRotation());
		}
		if(bCopyScale)
		{
			TargetTM.SetScale3D(TargetTM.GetScale3D() * (SourceTM.GetScale3D() * ScaleMultiplier));
		}
	}
	else //CopyMode = CopyBoneDeltaMode::Copy
	{
		if(bCopyTranslation)
		{
			TargetTM.SetTranslation(SourceTM.GetTranslation() * TranslationMultiplier);
		}

		if(bCopyRotation)
		{
			FVector Axis;
			float Angle;
			SourceTM.GetRotation().ToAxisAndAngle(Axis, Angle);

			TargetTM.SetRotation(FQuat(Axis, Angle * RotationMultiplier));
		}

		if(bCopyScale)
		{
			TargetTM.SetScale3D(SourceTM.GetScale3D() * ScaleMultiplier);
		}
	}

	// Back out to component space
	FAnimationRuntime::ConvertBoneSpaceTransformToCS(SkelComp, MeshBases, TargetTM, TargetBoneIndex, BCS_ParentBoneSpace);

	OutBoneTransforms.Add(FBoneTransform(TargetBoneIndex, TargetTM));
}

bool FAnimNode_CopyBoneDelta::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return (TargetBone.IsValid(RequiredBones) && (TargetBone == SourceBone || SourceBone.IsValid(RequiredBones)));
}

void FAnimNode_CopyBoneDelta::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	SourceBone.Initialize(RequiredBones);
	TargetBone.Initialize(RequiredBones);
}
