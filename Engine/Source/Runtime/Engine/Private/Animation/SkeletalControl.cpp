// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalControl.cpp: SkeletalControl code and related.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "AnimationUtils.h"
#include "AnimationRuntime.h"

DEFINE_LOG_CATEGORY(LogSkeletalControl);

/////////////////////////////////////////////////////
// UBoneMaskFilter 

UBoneMaskFilter::UBoneMaskFilter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/////////////////////////////////////////////////////
// FBoneReference

bool FBoneReference::Initialize(const FBoneContainer& RequiredBones)
{
	BoneName = *BoneName.ToString().Trim().TrimTrailing();
	BoneIndex = RequiredBones.GetPoseBoneIndexForBoneName(BoneName);

	// If bone name is not found, look into the master skeleton to see if it's found there.
	// SkeletalMeshes can exclude bones from the master skeleton, and that's OK.
	// If it's not found in the master skeleton, the bone does not exist at all! so we should report it as a warning.
	if( (BoneIndex == INDEX_NONE) && RequiredBones.GetSkeletonAsset() )
	{
		if( RequiredBones.GetSkeletonAsset()->GetReferenceSkeleton().FindBoneIndex(BoneName) == INDEX_NONE )
		{
			UE_LOG(LogAnimation, Warning, TEXT("FBoneReference::Initialize BoneIndex for Bone '%s' does not exist in Skeleton '%s'"), 
				*BoneName.ToString(), *GetNameSafe(RequiredBones.GetSkeletonAsset()));
		}
	}

	return (BoneIndex != INDEX_NONE);
}

bool FBoneReference::Initialize(const USkeleton* Skeleton)
{
	if( Skeleton && (BoneName != NAME_None) )
	{
		BoneName = *BoneName.ToString().Trim().TrimTrailing();
		BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
	}
	else
	{
		BoneIndex = INDEX_NONE;
	}

	return (BoneIndex != INDEX_NONE);
}

bool FBoneReference::IsValid(const FBoneContainer& RequiredBones) const
{
	return (BoneIndex != INDEX_NONE && RequiredBones.Contains(BoneIndex));
}

