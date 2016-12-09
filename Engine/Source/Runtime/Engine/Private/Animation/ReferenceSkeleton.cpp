// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ReferenceSkeleton.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"

FReferenceSkeletonModifier::~FReferenceSkeletonModifier()
{
	RefSkeleton.RebuildRefSkeleton(Skeleton, true);
}

void FReferenceSkeletonModifier::UpdateRefPoseTransform(const int32& BoneIndex, const FTransform& BonePose)
{
	RefSkeleton.UpdateRefPoseTransform(BoneIndex, BonePose);
}

void FReferenceSkeletonModifier::Add(const FMeshBoneInfo& BoneInfo, const FTransform& BonePose)
{
	RefSkeleton.Add(BoneInfo, BonePose);
}

int32 FReferenceSkeletonModifier::FindBoneIndex(const FName& BoneName) const
{
	return RefSkeleton.FindRawBoneIndex(BoneName);
}

const TArray<FMeshBoneInfo>& FReferenceSkeletonModifier::GetRefBoneInfo() const
{
	return RefSkeleton.GetRawRefBoneInfo();
}

FArchive &operator<<(FArchive& Ar, FMeshBoneInfo& F)
{
	Ar << F.Name << F.ParentIndex;

	if (Ar.IsLoading() && (Ar.UE4Ver() < VER_UE4_REFERENCE_SKELETON_REFACTOR))
	{
		FColor DummyColor = FColor::White;
		Ar << DummyColor;
	}

#if WITH_EDITORONLY_DATA
	if (Ar.UE4Ver() >= VER_UE4_STORE_BONE_EXPORT_NAMES)
	{
		if (!Ar.IsCooking() && !Ar.IsFilterEditorOnly())
		{
			Ar << F.ExportName;
		}
	}
	else
	{
		F.ExportName = F.Name.ToString();
	}
#endif

	return Ar;
}


//////////////////////////////////////////////////////////////////////////

FTransform GetComponentSpaceTransform(TArray<uint8>& ComponentSpaceFlags, TArray<FTransform>& ComponentSpaceTransforms, FReferenceSkeleton& RefSkeleton, int32 TargetIndex)
{
	FTransform& This = ComponentSpaceTransforms[TargetIndex];

	if (!ComponentSpaceFlags[TargetIndex])
	{
		const int32 ParentIndex = RefSkeleton.GetParentIndex(TargetIndex);
		This *= GetComponentSpaceTransform(ComponentSpaceFlags, ComponentSpaceTransforms, RefSkeleton, ParentIndex);
		ComponentSpaceFlags[TargetIndex] = 1;
	}
	return This;
}

void FReferenceSkeleton::RebuildRefSkeleton(const USkeleton* Skeleton, bool bRebuildNameMap)
{
	if (bRebuildNameMap)
	{
		//On loading FinalRefBone data wont exist but NameToIndexMap will and will be valid
		RebuildNameToIndexMap();
	}

	const int32 NumVirtualBones = Skeleton ? Skeleton->GetVirtualBones().Num() : 0;
	FinalRefBoneInfo = TArray<FMeshBoneInfo>(RawRefBoneInfo, NumVirtualBones);
	FinalRefBonePose = TArray<FTransform>(RawRefBonePose, NumVirtualBones);
	FinalNameToIndexMap = RawNameToIndexMap;

	RequiredVirtualBones.Reset(NumVirtualBones);
	UsedVirtualBoneData.Reset(NumVirtualBones);

	if (NumVirtualBones > 0)
	{
		TArray<uint8> ComponentSpaceFlags;
		ComponentSpaceFlags.AddZeroed(RawRefBonePose.Num());
		ComponentSpaceFlags[0] = 1;

		TArray<FTransform> ComponentSpaceTransforms = TArray<FTransform>(RawRefBonePose);

		for (int32 VirtualBoneIdx = 0; VirtualBoneIdx < NumVirtualBones; ++VirtualBoneIdx)
		{
			const int32 ActualIndex = VirtualBoneIdx + RawRefBoneInfo.Num();
			const FVirtualBone& VB = Skeleton->GetVirtualBones()[VirtualBoneIdx];

			const int32 ParentIndex = FindBoneIndex(VB.SourceBoneName);
			const int32 TargetIndex = FindBoneIndex(VB.TargetBoneName);
			if(ParentIndex != INDEX_NONE && TargetIndex != INDEX_NONE)
			{
				FinalRefBoneInfo.Add(FMeshBoneInfo(VB.VirtualBoneName, VB.VirtualBoneName.ToString(), ParentIndex));

				const FTransform TargetCS = GetComponentSpaceTransform(ComponentSpaceFlags, ComponentSpaceTransforms, *this, TargetIndex);
				const FTransform SourceCS = GetComponentSpaceTransform(ComponentSpaceFlags, ComponentSpaceTransforms, *this, ParentIndex);

				FTransform VBTransform = TargetCS.GetRelativeTransform(SourceCS);

				const int32 NewBoneIndex = FinalRefBonePose.Add(VBTransform);
				FinalNameToIndexMap.Add(VB.VirtualBoneName) = NewBoneIndex;
				RequiredVirtualBones.Add(NewBoneIndex);
				UsedVirtualBoneData.Add(FVirtualBoneRefData(NewBoneIndex, ParentIndex, TargetIndex));
			}
		}
	}
}

void FReferenceSkeleton::RemoveDuplicateBones(const UObject* Requester, TArray<FBoneIndexType> & DuplicateBones)
{
	//Process raw bone data only
	const int32 NumBones = RawRefBoneInfo.Num();
	DuplicateBones.Empty();

	TMap<FName, int32> BoneNameCheck;
	bool bRemovedBones = false;
	for (int32 BoneIndex = NumBones - 1; BoneIndex >= 0; BoneIndex--)
	{
		const FName& BoneName = GetBoneName(BoneIndex);
		const int32* FoundBoneIndexPtr = BoneNameCheck.Find(BoneName);

		// Not a duplicate bone, track it.
		if (FoundBoneIndexPtr == NULL)
		{
			BoneNameCheck.Add(BoneName, BoneIndex);
		}
		else
		{
			const int32 DuplicateBoneIndex = *FoundBoneIndexPtr;
			DuplicateBones.Add(DuplicateBoneIndex);

			UE_LOG(LogAnimation, Warning, TEXT("RemoveDuplicateBones: duplicate bone name (%s) detected for (%s)! Indices: %d and %d. Removing the latter."),
				*BoneName.ToString(), *GetNameSafe(Requester), DuplicateBoneIndex, BoneIndex);

			// Remove duplicate bone index, which was added later as a mistake.
			RawRefBonePose.RemoveAt(DuplicateBoneIndex, 1);
			RawRefBoneInfo.RemoveAt(DuplicateBoneIndex, 1);

			// Now we need to fix all the parent indices that pointed to bones after this in the array
			// These must be after this point in the array.
			for (int32 j = DuplicateBoneIndex; j < GetRawBoneNum(); j++)
			{
				if (GetParentIndex(j) >= DuplicateBoneIndex)
				{
					RawRefBoneInfo[j].ParentIndex -= 1;
				}
			}

			// Update entry in case problem bones were added multiple times.
			BoneNameCheck.Add(BoneName, BoneIndex);

			// We need to make sure that any bone that has this old bone as a parent is fixed up
			bRemovedBones = true;
		}
	}

	// If we've removed bones, we need to rebuild our name table.
	if (bRemovedBones || (RawNameToIndexMap.Num() == 0))
	{
		const USkeleton* Skeleton = Cast<USkeleton>(Requester);
		if (!Skeleton)
		{
			if (const USkeletalMesh* Mesh = Cast<USkeletalMesh>(Requester))
			{
				Skeleton = Mesh->Skeleton;
			}
			else
			{
				UE_LOG(LogAnimation, Warning, TEXT("RemoveDuplicateBones: Object supplied as requester (%s) needs to be either Skeleton or SkeletalMesh"), *GetFullNameSafe(Requester));
			}
		}

		// Additionally normalize all quaternions to be safe.
		for (int32 BoneIndex = 0; BoneIndex < GetRawBoneNum(); BoneIndex++)
		{
			RawRefBonePose[BoneIndex].NormalizeRotation();
		}

		const bool bRebuildNameMap = true;
		RebuildRefSkeleton(Skeleton, bRebuildNameMap);
	}

	// Make sure our arrays are in sync.
	checkSlow((RawRefBoneInfo.Num() == RawRefBonePose.Num()) && (RawRefBoneInfo.Num() == RawNameToIndexMap.Num()));
}

void FReferenceSkeleton::RebuildNameToIndexMap()
{
	// Start by clearing the current map.
	RawNameToIndexMap.Empty();

	// Then iterate over each bone, adding the name and bone index.
	const int32 NumBones = RawRefBoneInfo.Num();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; BoneIndex++)
	{
		const FName& BoneName = RawRefBoneInfo[BoneIndex].Name;
		if (BoneName != NAME_None)
		{
			RawNameToIndexMap.Add(BoneName, BoneIndex);
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("RebuildNameToIndexMap: Bone with no name detected for index: %d"), BoneIndex);
		}
	}

	// Make sure we don't have duplicate bone names. This would be very bad.
	checkSlow(RawNameToIndexMap.Num() == NumBones);
}

SIZE_T FReferenceSkeleton::GetDataSize() const
{
	SIZE_T ResourceSize = 0;

	ResourceSize += RawRefBoneInfo.GetAllocatedSize();
	ResourceSize += RawRefBonePose.GetAllocatedSize();

	ResourceSize += FinalRefBoneInfo.GetAllocatedSize();
	ResourceSize += FinalRefBonePose.GetAllocatedSize();

	ResourceSize += RawNameToIndexMap.GetAllocatedSize();
	ResourceSize += FinalNameToIndexMap.GetAllocatedSize();

	return ResourceSize;
}

FArchive & operator<<(FArchive & Ar, FReferenceSkeleton & F)
{
	Ar << F.RawRefBoneInfo;
	Ar << F.RawRefBonePose;

	if (Ar.UE4Ver() >= VER_UE4_REFERENCE_SKELETON_REFACTOR)
	{
		Ar << F.RawNameToIndexMap;
	}

	// Fix up any assets that don't have an INDEX_NONE parent for Bone[0]
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FIXUP_ROOTBONE_PARENT)
	{
		if ((F.RawRefBoneInfo.Num() > 0) && (F.RawRefBoneInfo[0].ParentIndex != INDEX_NONE))
		{
			F.RawRefBoneInfo[0].ParentIndex = INDEX_NONE;
		}
	}

	if (Ar.IsLoading())
	{
		F.FinalRefBoneInfo = F.RawRefBoneInfo;
		F.FinalRefBonePose = F.RawRefBonePose;
		F.FinalNameToIndexMap = F.RawNameToIndexMap;
	}

	return Ar;
}

