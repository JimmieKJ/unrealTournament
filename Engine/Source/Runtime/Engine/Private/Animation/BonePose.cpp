// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "BonePose.h"
#include "AnimationRuntime.h"

void FMeshPose::ResetToRefPose()
{
	FAnimationRuntime::FillWithRefPose(Bones, *BoneContainer);
}

void FMeshPose::ResetToIdentity()
{
	FAnimationRuntime::InitializeTransform(*BoneContainer, Bones);
}

void FMeshPose::InitFrom(FCompactPose& CompactPose)
{
	SetBoneContainer(&CompactPose.GetBoneContainer());
	for (const FCompactPoseBoneIndex BoneIndex : CompactPose.ForEachBoneIndex())
	{
		FMeshPoseBoneIndex MeshPoseBoneIndex = GetBoneContainer().MakeMeshPoseIndex(BoneIndex);
		Bones[MeshPoseBoneIndex.GetInt()] = CompactPose[BoneIndex];
	}
}

bool FMeshPose::ContainsNaN() const
{
	const TArray<FBoneIndexType> & RequiredBoneIndices = BoneContainer->GetBoneIndicesArray();
	for (int32 Iter = 0; Iter < RequiredBoneIndices.Num(); ++Iter)
	{
		const int32 BoneIndex = RequiredBoneIndices[Iter];
		if (Bones[BoneIndex].ContainsNaN())
		{
			return true;
		}
	}

	return false;
}

bool FMeshPose::IsNormalized() const
{
	const TArray<FBoneIndexType> & RequiredBoneIndices = BoneContainer->GetBoneIndicesArray();
	for (int32 Iter = 0; Iter < RequiredBoneIndices.Num(); ++Iter)
	{
		int32 BoneIndex = RequiredBoneIndices[Iter];
		const FTransform& Trans = Bones[BoneIndex];
		if (!Bones[BoneIndex].IsRotationNormalized())
		{
			return false;
		}
	}

	return true;
}

void FCompactPose::ResetToRefPose(const FBoneContainer& RequiredBones)
{
	const TArray<FTransform>& RefPoseCompactArray = RequiredBones.GetRefPoseCompactArray();
	Bones.Reset();
	Bones.Append(RefPoseCompactArray);

	// If retargeting is disabled, copy ref pose from Skeleton, rather than mesh.
	// this is only used in editor and for debugging.
	if (RequiredBones.GetDisableRetargeting())
	{
		checkSlow(RequiredBones.IsValid());
		// Only do this if we have a mesh. otherwise we're not retargeting animations.
		if (RequiredBones.GetSkeletalMeshAsset())
		{
			TArray<FTransform> const & SkeletonRefPose = RequiredBones.GetSkeletonAsset()->GetRefLocalPoses();

			for (const FCompactPoseBoneIndex BoneIndex : ForEachBoneIndex())
			{
				const int32 SkeletonBoneIndex = GetBoneContainer().GetSkeletonIndex(BoneIndex);

				// Pose bone index should always exist in Skeleton
				checkSlow(SkeletonBoneIndex != INDEX_NONE);
				Bones[BoneIndex.GetInt()] = SkeletonRefPose[SkeletonBoneIndex];
			}
		}
	}
}

void FCompactPose::ResetToIdentity()
{
	for (FTransform& Bone : Bones)
	{
		Bone.SetIdentity();
	}
}

bool FCompactPose::IsNormalized() const
{
	for (const FTransform& Bone : Bones)
	{
		if (!Bone.IsRotationNormalized())
		{
			return false;
		}
	}

	return true;
}

bool FCompactPose::ContainsNaN() const
{
	for (const FTransform& Bone : Bones)
	{
		if (Bone.ContainsNaN())
		{
			return true;
		}
	}

	return false;
}

void FCompactPose::NormalizeRotations()
{
	for (FTransform& Bone : Bones)
	{
		Bone.NormalizeRotation();
	}
}
