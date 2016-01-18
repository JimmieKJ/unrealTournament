// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ReferenceSkeleton.h"

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

void FReferenceSkeleton::RemoveDuplicateBones(const UObject* Requester, TArray<FBoneIndexType> & DuplicateBones)
{
	const int32 NumBones = GetNum();
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
			RefBonePose.RemoveAt(DuplicateBoneIndex, 1);
			RefBoneInfo.RemoveAt(DuplicateBoneIndex, 1);

			// Now we need to fix all the parent indices that pointed to bones after this in the array
			// These must be after this point in the array.
			for (int32 j = DuplicateBoneIndex; j < GetNum(); j++)
			{
				if (GetParentIndex(j) >= DuplicateBoneIndex)
				{
					RefBoneInfo[j].ParentIndex -= 1;
				}
			}

			// Update entry in case problem bones were added multiple times.
			BoneNameCheck.Add(BoneName, BoneIndex);

			// We need to make sure that any bone that has this old bone as a parent is fixed up
			bRemovedBones = true;
		}
	}

	// If we've removed bones, we need to rebuild our name table.
	if (bRemovedBones || (NameToIndexMap.Num() == 0))
	{
		RebuildNameToIndexMap();
	}

	// Make sure our arrays are in sync.
	checkSlow((RefBoneInfo.Num() == RefBonePose.Num()) && (RefBoneInfo.Num() == NameToIndexMap.Num()));

	// Additionally normalize all quaternions to be safe.
	for (int32 BoneIndex = 0; BoneIndex < GetNum(); BoneIndex++)
	{
		RefBonePose[BoneIndex].NormalizeRotation();
	}
}

void FReferenceSkeleton::RebuildNameToIndexMap()
{
	// Start by clearing the current map.
	NameToIndexMap.Empty();

	// Then iterate over each bone, adding the name and bone index.
	const int32 NumBones = GetNum();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; BoneIndex++)
	{
		const FName& BoneName = GetBoneName(BoneIndex);
		if (BoneName != NAME_None)
		{
			NameToIndexMap.Add(BoneName, BoneIndex);
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("RebuildNameToIndexMap: Bone with no name detected for index: %d"), BoneIndex);
		}
	}

	// Make sure we don't have duplicate bone names. This would be very bad.
	checkSlow(NameToIndexMap.Num() == NumBones);
}

SIZE_T FReferenceSkeleton::GetDataSize() const
{
	SIZE_T ResourceSize = 0;

	ResourceSize += RefBoneInfo.GetAllocatedSize();
	ResourceSize += RefBonePose.GetAllocatedSize();
	ResourceSize += NameToIndexMap.GetAllocatedSize();

	return ResourceSize;
}

FArchive & operator<<(FArchive & Ar, FReferenceSkeleton & F)
{
	Ar << F.RefBoneInfo;
	Ar << F.RefBonePose;

	if (Ar.UE4Ver() >= VER_UE4_REFERENCE_SKELETON_REFACTOR)
	{
		Ar << F.NameToIndexMap;
	}

	// Fix up any assets that don't have an INDEX_NONE parent for Bone[0]
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FIXUP_ROOTBONE_PARENT)
	{
		if ((F.RefBoneInfo.Num() > 0) && (F.RefBoneInfo[0].ParentIndex != INDEX_NONE))
		{
			F.RefBoneInfo[0].ParentIndex = INDEX_NONE;
		}
	}

	return Ar;
}

