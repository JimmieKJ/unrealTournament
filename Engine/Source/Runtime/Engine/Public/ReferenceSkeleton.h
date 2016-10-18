// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneIndices.h"

// This contains Reference-skeleton related info
// Bone transform is saved as FTransform array
struct FMeshBoneInfo
{
	// Bone's name.
	FName Name;

	// 0/NULL if this is the root bone. 
	int32 ParentIndex;

#if WITH_EDITORONLY_DATA
	// Name used for export (this should be exact as FName may mess with case) 
	FString ExportName;
#endif

	FMeshBoneInfo() : Name(NAME_None), ParentIndex(INDEX_NONE) {}

	FMeshBoneInfo(const FName& InName, const FString& InExportName, int32 InParentIndex)
		: Name(InName)
		, ParentIndex(InParentIndex)
#if WITH_EDITORONLY_DATA
		, ExportName(InExportName)
#endif
	{}

	FMeshBoneInfo(const FMeshBoneInfo& Other)
		: Name(Other.Name)
		, ParentIndex(Other.ParentIndex)
#if WITH_EDITORONLY_DATA
		, ExportName(Other.ExportName)
#endif
	{}

	bool operator==(const FMeshBoneInfo& B) const
	{
		return(Name == B.Name);
	}

	friend FArchive &operator<<(FArchive& Ar, FMeshBoneInfo& F);
};

/** Reference Skeleton **/
struct FReferenceSkeleton
{
private:
	/** Reference bone related info to be serialized **/
	TArray<FMeshBoneInfo>	RefBoneInfo;
	/** Reference bone transform **/
	TArray<FTransform>		RefBonePose;
	/** TMap to look up bone index from bone name. */
	TMap<FName, int32>		NameToIndexMap;

	/** Removes the specified bone, so long as it has no children. Returns whether we removed the bone or not */
	bool RemoveIndividualBone(int32 BoneIndex, TArray<int32>& OutBonesRemoved)
	{
		bool bRemoveThisBone = true;

		// Make sure we have no children
		for(int32 CurrBoneIndex=BoneIndex+1; CurrBoneIndex < GetNum(); CurrBoneIndex++)
		{
			if( RefBoneInfo[CurrBoneIndex].ParentIndex == BoneIndex )
			{
				bRemoveThisBone = false;
				break;
			}
		}

		if(bRemoveThisBone)
		{
			// Update parent indices of bones further through the array
			for(int32 CurrBoneIndex=BoneIndex+1; CurrBoneIndex < GetNum(); CurrBoneIndex++)
			{
				FMeshBoneInfo& Bone = RefBoneInfo[CurrBoneIndex];
				if( Bone.ParentIndex > BoneIndex )
				{
					Bone.ParentIndex -= 1;
				}
			}

			OutBonesRemoved.Add(BoneIndex);
			RefBonePose.RemoveAt(BoneIndex, 1);
			RefBoneInfo.RemoveAt(BoneIndex, 1);
		}
		return bRemoveThisBone;
	}

public:
	void Allocate(int32 Size)
	{
		NameToIndexMap.Empty(Size);
		RefBoneInfo.Empty(Size);
		RefBonePose.Empty(Size);
	}

	/** Returns number of bones in Skeleton. */
	int32 GetNum() const
	{
		return RefBoneInfo.Num();
	}

	/** Accessor to private data. Const so it can't be changed recklessly. */
	const TArray<FMeshBoneInfo> & GetRefBoneInfo() const
	{
		return RefBoneInfo;
	}

	/** Accessor to private data. Const so it can't be changed recklessly. */
	const TArray<FTransform> & GetRefBonePose() const
	{
		return RefBonePose;
	}

	void UpdateRefPoseTransform(const int32& BoneIndex, const FTransform& BonePose)
	{
		RefBonePose[BoneIndex] = BonePose;
	}

	/** Add a new bone. 
	 * BoneName must not already exist! ParentIndex must be valid. */
	void Add(const FMeshBoneInfo& BoneInfo, const FTransform& BonePose)
	{
		// Adding a bone that already exists is illegal
		check(FindBoneIndex(BoneInfo.Name) == INDEX_NONE);
	
		// Make sure our arrays are in sync.
		checkSlow( (RefBoneInfo.Num() == RefBonePose.Num()) && (RefBoneInfo.Num() == NameToIndexMap.Num()) );

		const int32 BoneIndex = RefBoneInfo.Add(BoneInfo);
		RefBonePose.Add(BonePose);
		NameToIndexMap.Add(BoneInfo.Name, BoneIndex);

		// Normalize Quaternion to be safe.
		RefBonePose[BoneIndex].NormalizeRotation();

		// Parent must be valid. Either INDEX_NONE for Root, or before us.
		check( ((BoneIndex == 0) && (BoneInfo.ParentIndex == INDEX_NONE)) || ((BoneIndex > 0) && RefBoneInfo.IsValidIndex(BoneInfo.ParentIndex)) );
	}

	void Empty()
	{
		RefBoneInfo.Empty();
		RefBonePose.Empty();
		NameToIndexMap.Empty();
	}

	/** Find Bone Index from BoneName. Precache as much as possible in speed critical sections! */
	int32 FindBoneIndex(const FName& BoneName) const
	{
		checkSlow(RefBoneInfo.Num() == NameToIndexMap.Num());
		int32 BoneIndex = INDEX_NONE;
		if( BoneName != NAME_None )
		{
			const int32* IndexPtr = NameToIndexMap.Find(BoneName);
			if( IndexPtr )
			{
				BoneIndex = *IndexPtr;
			}
		}
		return BoneIndex;
	}

	FName GetBoneName(const int32& BoneIndex) const
	{
		return RefBoneInfo[BoneIndex].Name;
	}

	int32 GetParentIndex(const int32& BoneIndex) const
	{
		// Parent must be valid. Either INDEX_NONE for Root, or before us.
		checkSlow( ((BoneIndex == 0) && (RefBoneInfo[BoneIndex].ParentIndex == INDEX_NONE)) || ((BoneIndex > 0) && RefBoneInfo.IsValidIndex(RefBoneInfo[BoneIndex].ParentIndex)) );
		return RefBoneInfo[BoneIndex].ParentIndex;
	}

	bool IsValidIndex(int32 Index) const
	{
		return (RefBoneInfo.IsValidIndex(Index));
	}

	/** 
	 * Returns # of Depth from BoneIndex to ParentBoneIndex
	 * This will return 0 if BoneIndex == ParentBoneIndex;
	 * This will return -1 if BoneIndex isn't child of ParentBoneIndex
	 */
	int32 GetDepthBetweenBones(const int32& BoneIndex, const int32& ParentBoneIndex) const
	{
		if (BoneIndex >= ParentBoneIndex)
		{
			int32 CurBoneIndex = BoneIndex;
			int32 Depth = 0;

			do
			{
				// if same return;
				if (CurBoneIndex == ParentBoneIndex)
				{
					return Depth;
				}

				CurBoneIndex = RefBoneInfo[CurBoneIndex].ParentIndex;
				++Depth;

			} while (CurBoneIndex!=INDEX_NONE);
		}

		return INDEX_NONE;
	}

	bool BoneIsChildOf(const int32& ChildBoneIndex, const int32& ParentBoneIndex) const
	{
		// Bones are in strictly increasing order.
		// So child must have an index greater than his parent.
		if( ChildBoneIndex > ParentBoneIndex )
		{
			int32 BoneIndex = GetParentIndex(ChildBoneIndex);
			do
			{
				if( BoneIndex == ParentBoneIndex )
				{
					return true;
				}
				BoneIndex = GetParentIndex(BoneIndex);

			} while (BoneIndex != INDEX_NONE);
		}

		return false;
	}

	void RemoveDuplicateBones(const UObject* Requester, TArray<FBoneIndexType> & DuplicateBones);


	/** Removes the supplied bones from the skeleton, unless they have children that aren't also going to be removed */
	TArray<int32> RemoveBonesByName(const TArray<FName>& BonesToRemove)
	{
		TArray<int32> BonesRemoved;

		const int32 NumBones = GetNum();
		for(int32 BoneIndex=NumBones-1; BoneIndex>=0; BoneIndex--)
		{
			FMeshBoneInfo& Bone = RefBoneInfo[BoneIndex];

			if(BonesToRemove.Contains(Bone.Name))
			{
				RemoveIndividualBone(BoneIndex, BonesRemoved);
			}
		}
		RebuildNameToIndexMap();
		return BonesRemoved;
	}

	void RebuildNameToIndexMap();

	SIZE_T GetDataSize() const;

	friend FArchive & operator<<(FArchive & Ar, FReferenceSkeleton & F);
};