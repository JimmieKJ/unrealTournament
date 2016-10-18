// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Engine.h"
#include "MeshBoneReduction.h"
#include "Developer/MeshUtilities/Public/MeshUtilities.h"
#include "ComponentReregisterContext.h"

class FMeshBoneReductionModule : public IMeshBoneReductionModule
{
public:
	// IModuleInterface interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IMeshBoneReductionModule interface.
	virtual class IMeshBoneReduction* GetMeshBoneReductionInterface() override;
};

DEFINE_LOG_CATEGORY_STATIC(LogMeshBoneReduction, Log, All);
IMPLEMENT_MODULE(FMeshBoneReductionModule, IMeshBoneReductionModule);

class FMeshBoneReduction : public IMeshBoneReduction
{
public:

	virtual ~FMeshBoneReduction()
	{
	}

	void EnsureChildrenPresents(FBoneIndexType BoneIndex, const TArray<FMeshBoneInfo>& RefBoneInfo, TArray<FBoneIndexType>& OutBoneIndicesToRemove)
	{
		// just look for direct parent, we could look for RefBoneInfo->Ischild, but more expensive, and no reason to do that all the work
		for (int32 ChildBoneIndex = 0; ChildBoneIndex < RefBoneInfo.Num(); ++ChildBoneIndex)
		{
			if (RefBoneInfo[ChildBoneIndex].ParentIndex == BoneIndex)
			{
				OutBoneIndicesToRemove.AddUnique(ChildBoneIndex);
				EnsureChildrenPresents(ChildBoneIndex, RefBoneInfo, OutBoneIndicesToRemove);
			}
		}
	}

	bool GetBoneReductionData(const USkeletalMesh* SkeletalMesh, int32 DesiredLOD, TMap<FBoneIndexType, FBoneIndexType>& OutBonesToReplace, const TArray<FName>* BoneNamesToRemove = NULL) override
	{
		if (!SkeletalMesh)
		{
			return false;
		}

		if (!SkeletalMesh->LODInfo.IsValidIndex(DesiredLOD))
		{
			return false;
		}

		const TArray<FMeshBoneInfo> & RefBoneInfo = SkeletalMesh->RefSkeleton.GetRefBoneInfo();
		TArray<FBoneIndexType> BoneIndicesToRemove;

		// originally this code was accumulating from LOD 0->DesiredLOd, but that should be done outside of tool if they want to
		// removing it, and just include DesiredLOD
		{
			// if name is entered, use them instead of setting
			const TArray<FName>& BonesToRemoveSetting = (BoneNamesToRemove) ? *BoneNamesToRemove : SkeletalMesh->LODInfo[DesiredLOD].RemovedBones;

			// first gather indices. we don't want to add bones to replace if that "to-be-replace" will be removed as well
			for (int32 Index = 0; Index < BonesToRemoveSetting.Num(); ++Index)
			{
				int32 BoneIndex = SkeletalMesh->RefSkeleton.FindBoneIndex(BonesToRemoveSetting[Index]);

				// we don't allow root to be removed
				if ( BoneIndex > 0 )
				{
					BoneIndicesToRemove.AddUnique(BoneIndex);
					// make sure all children for this joint is included
					EnsureChildrenPresents(BoneIndex, RefBoneInfo, BoneIndicesToRemove);
				}
			}
		}

		if (BoneIndicesToRemove.Num() <= 0)
		{
			return false;
		}

		// now make sure the parent isn't the one to be removed, find the one that won't be removed
		for (int32 Index = 0; Index < BoneIndicesToRemove.Num(); ++Index)
		{
			int32 BoneIndex = BoneIndicesToRemove[Index];
			int32 ParentIndex = RefBoneInfo[BoneIndex].ParentIndex;

			while (BoneIndicesToRemove.Contains(ParentIndex))
			{
				ParentIndex = RefBoneInfo[ParentIndex].ParentIndex;
			}

			OutBonesToReplace.Add(BoneIndex, ParentIndex);
		}

		return ( OutBonesToReplace.Num() > 0 );
	}

	void FixUpSectionBoneMaps( FSkelMeshSection & Section, const TMap<FBoneIndexType, FBoneIndexType> &BonesToRepair ) override
	{
		// now you have list of bones, remove them from vertex influences
		{
			TMap<uint8, uint8> BoneMapRemapTable;
			// first go through bone map and see if this contains BonesToRemove
			int32 BoneMapSize = Section.BoneMap.Num();
			int32 AdjustIndex=0;

			for (int32 BoneMapIndex=0; BoneMapIndex < BoneMapSize; ++BoneMapIndex )
			{
				// look for this bone to be removed or not?
				const FBoneIndexType* ParentBoneIndex = BonesToRepair.Find(Section.BoneMap[BoneMapIndex]);
				if ( ParentBoneIndex  )
				{
					// this should not happen, I don't ever remove root
					check (*ParentBoneIndex!=INDEX_NONE);

					// if Parent already exists in the current BoneMap, we just have to fix up the mapping
					int32 ParentBoneMapIndex = Section.BoneMap.Find(*ParentBoneIndex);

					// if it exists
					if (ParentBoneMapIndex != INDEX_NONE)
					{
						// if parent index is higher, we have to decrease it to match to new index
						if (ParentBoneMapIndex > BoneMapIndex)
						{
							--ParentBoneMapIndex;
						}

						// remove current section count, will replace with parent
						Section.BoneMap.RemoveAt(BoneMapIndex);
					}
					else
					{
						// if parent doens't exists, we have to add one
						// this doesn't change bone map size 
						Section.BoneMap.RemoveAt(BoneMapIndex);
						ParentBoneMapIndex = Section.BoneMap.Add(*ParentBoneIndex);
					}

					// first fix up all indices of BoneMapRemapTable for the indices higher than BoneMapIndex, since BoneMapIndex is being removed
					for (auto Iter = BoneMapRemapTable.CreateIterator(); Iter; ++Iter)
					{
						uint8& Value = Iter.Value();

						check (Value != BoneMapIndex);
						if (Value > BoneMapIndex)
						{
							--Value;
						}
					}

					int32 OldIndex = BoneMapIndex+AdjustIndex;
					int32 NewIndex = ParentBoneMapIndex;
					// you still have to add no matter what even if same since indices might change after added
					{
						// add to remap table
						check (OldIndex < 256 && OldIndex >= 0);
						check (NewIndex < 256 && NewIndex >= 0);
						check (BoneMapRemapTable.Contains((uint8)OldIndex) == false);
						BoneMapRemapTable.Add((uint8)OldIndex, (uint8)NewIndex);
					}

					// reduce index since the item is removed
					--BoneMapIndex;
					--BoneMapSize;

					// this is to adjust the later indices. We need to refix their indices
					++AdjustIndex;
				}
				else if (AdjustIndex > 0)
				{
					int32 OldIndex = BoneMapIndex+AdjustIndex;
					int32 NewIndex = BoneMapIndex;

					check (OldIndex < 256 && OldIndex >= 0);
					check (NewIndex < 256 && NewIndex >= 0);
					check (BoneMapRemapTable.Contains((uint8)OldIndex) == false);
					BoneMapRemapTable.Add((uint8)OldIndex, (uint8)NewIndex);
				}
			}

			if ( BoneMapRemapTable.Num() > 0 )
			{
				// fix up soft verts
				for (int32 VertIndex=0; VertIndex < Section.SoftVertices.Num(); ++VertIndex)
				{
					FSoftSkinVertex & Vert = Section.SoftVertices[VertIndex];
					bool ShouldRenormalize = false;

					for(int32 InfluenceIndex = 0;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
					{
						uint8 *RemappedBone = BoneMapRemapTable.Find(Vert.InfluenceBones[InfluenceIndex]);
						if (RemappedBone)
						{
							Vert.InfluenceBones[InfluenceIndex] = *RemappedBone;
							ShouldRenormalize = true;
						}
					}

					if (ShouldRenormalize)
					{
						// should see if same bone exists
						for(int32 InfluenceIndex = 0;InfluenceIndex < MAX_TOTAL_INFLUENCES;InfluenceIndex++)
						{
							for(int32 InfluenceIndex2 = InfluenceIndex+1;InfluenceIndex2 < MAX_TOTAL_INFLUENCES;InfluenceIndex2++)
							{
								// cannot be 0 because we don't allow removing root
								if (Vert.InfluenceBones[InfluenceIndex] != 0 && Vert.InfluenceBones[InfluenceIndex] == Vert.InfluenceBones[InfluenceIndex2])
								{
									Vert.InfluenceWeights[InfluenceIndex] += Vert.InfluenceWeights[InfluenceIndex2];
									// reset
									Vert.InfluenceBones[InfluenceIndex2] = 0;
									Vert.InfluenceWeights[InfluenceIndex2] = 0;
								}
							}
						}
					}
				}
			}
		}
	}

	bool ReduceBoneCounts(USkeletalMesh* SkeletalMesh, int32 DesiredLOD, const TArray<FName>* BoneNamesToRemove) override
	{
		check (SkeletalMesh);
		USkeleton* Skeleton = SkeletalMesh->Skeleton;
		check (Skeleton);

		// find all the bones to remove from Skeleton settings
		TMap<FBoneIndexType, FBoneIndexType> BonesToRemove;

		if (GetBoneReductionData(SkeletalMesh, DesiredLOD, BonesToRemove, BoneNamesToRemove) == false)
		{
			return false;
		}

		TComponentReregisterContext<USkinnedMeshComponent> ReregisterContext;
		SkeletalMesh->ReleaseResources();
		SkeletalMesh->ReleaseResourcesFence.Wait();

		FSkeletalMeshResource* SkeletalMeshResource = SkeletalMesh->GetImportedResource();
		check(SkeletalMeshResource);

		FStaticLODModel** LODModels = SkeletalMeshResource->LODModels.GetData();
		FStaticLODModel* SrcModel = LODModels[DesiredLOD];
		FStaticLODModel* NewModel = new FStaticLODModel();
		LODModels[DesiredLOD] = NewModel;

		// Bulk data arrays need to be locked before a copy can be made.
		SrcModel->RawPointIndices.Lock( LOCK_READ_ONLY );
		SrcModel->LegacyRawPointIndices.Lock( LOCK_READ_ONLY );
		*NewModel = *SrcModel;
		SrcModel->RawPointIndices.Unlock();
		SrcModel->LegacyRawPointIndices.Unlock();

		// The index buffer needs to be rebuilt on copy.
		FMultiSizeIndexContainerData IndexBufferData, AdjacencyIndexBufferData;
		SrcModel->MultiSizeIndexContainer.GetIndexBufferData( IndexBufferData );
		SrcModel->AdjacencyMultiSizeIndexContainer.GetIndexBufferData( AdjacencyIndexBufferData );
		NewModel->RebuildIndexBuffer( &IndexBufferData, &AdjacencyIndexBufferData );

		// fix up chunks
		for ( int32 SectionIndex=0; SectionIndex< NewModel->Sections.Num(); ++SectionIndex)
		{
			FixUpSectionBoneMaps(NewModel->Sections[SectionIndex], BonesToRemove);
		}

		// fix up RequiredBones/ActiveBoneIndices
		for(auto Iter = BonesToRemove.CreateIterator(); Iter; ++Iter)
		{
			FBoneIndexType BoneIndex = Iter.Key();
			FBoneIndexType MappingIndex = Iter.Value();
			NewModel->ActiveBoneIndices.Remove(BoneIndex);
			NewModel->RequiredBones.Remove(BoneIndex);

			NewModel->ActiveBoneIndices.AddUnique(MappingIndex);
			NewModel->RequiredBones.AddUnique(MappingIndex);
		}

		NewModel->ActiveBoneIndices.Sort();
		NewModel->RequiredBones.Sort();

		SkeletalMesh->PostEditChange();
		SkeletalMesh->InitResources();
		SkeletalMesh->MarkPackageDirty();

		delete SrcModel;

		return true;
	}
};

TScopedPointer<FMeshBoneReduction> GMeshBoneReduction;

void FMeshBoneReductionModule::StartupModule()
{
	GMeshBoneReduction = new FMeshBoneReduction();
}

void FMeshBoneReductionModule::ShutdownModule()
{
	GMeshBoneReduction = NULL;
}

IMeshBoneReduction* FMeshBoneReductionModule::GetMeshBoneReductionInterface()
{
	return GMeshBoneReduction;
}
