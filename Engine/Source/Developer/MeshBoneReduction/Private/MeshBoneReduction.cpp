// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	bool GetBoneReductionData( const USkeletalMesh* SkeletalMesh, int32 DesiredLOD, TMap<FBoneIndexType, FBoneIndexType> &OutBonesToReplace ) override
	{
		const TArray<FMeshBoneInfo> & RefBoneInfo = SkeletalMesh->RefSkeleton.GetRefBoneInfo();

		USkeleton* Skeleton = SkeletalMesh->Skeleton;
		if (Skeleton)
		{
			TArray<FBoneIndexType> BoneIndicesToRemove;
			// it accumulate from LOD 0 -> LOD N if N+1 is DesiredLOD
			// since we don't like to keep the bones that weren't included in (LOD-1)
			for ( int LODIndex=0; LODIndex < DesiredLOD && Skeleton->BoneReductionSettingsForLODs.Num() > LODIndex; ++LODIndex )
			{
				// first gather indices. we don't want to add bones to replace if that "to-be-replace" will be removed as well
				for (int32 Index = 0; Index < Skeleton->BoneReductionSettingsForLODs[LODIndex].BonesToRemove.Num(); ++Index)
				{
					int32 BoneIndex = SkeletalMesh->RefSkeleton.FindBoneIndex(Skeleton->BoneReductionSettingsForLODs[LODIndex].BonesToRemove[Index]);

					// we don't allow root to be removed
					if ( BoneIndex > 0 )
					{
						BoneIndicesToRemove.AddUnique(BoneIndex);
					}
				}

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
		}

		return ( OutBonesToReplace.Num() > 0 );
	}

	void FixUpChunkBoneMaps( FSkelMeshChunk & Chunk, const TMap<FBoneIndexType, FBoneIndexType> &BonesToRepair ) override
	{
		// now you have list of bones, remove them from vertex influences
		{
			TMap<uint8, uint8> BoneMapRemapTable;
			// first go through bone map and see if this contains BonesToRemove
			int32 BoneMapSize = Chunk.BoneMap.Num();
			int32 AdjustIndex=0;

			for (int32 BoneMapIndex=0; BoneMapIndex < BoneMapSize; ++BoneMapIndex )
			{
				// look for this bone to be removed or not?
				const FBoneIndexType* ParentBoneIndex = BonesToRepair.Find(Chunk.BoneMap[BoneMapIndex]);
				if ( ParentBoneIndex  )
				{
					// this should not happen, I don't ever remove root
					check (*ParentBoneIndex!=INDEX_NONE);

					// if Parent already exists in the current BoneMap, we just have to fix up the mapping
					int32 ParentBoneMapIndex = Chunk.BoneMap.Find(*ParentBoneIndex);

					// if it exists
					if (ParentBoneMapIndex != INDEX_NONE)
					{
						// if parent index is higher, we have to decrease it to match to new index
						if (ParentBoneMapIndex > BoneMapIndex)
						{
							--ParentBoneMapIndex;
						}

						// remove current chunk count, will replace with parent
						Chunk.BoneMap.RemoveAt(BoneMapIndex);
					}
					else
					{
						// if parent doens't exists, we have to add one
						// this doesn't change bone map size 
						Chunk.BoneMap.RemoveAt(BoneMapIndex);
						ParentBoneMapIndex = Chunk.BoneMap.Add(*ParentBoneIndex);
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
				// fix up rigid verts
				for (int32 VertIndex=0; VertIndex < Chunk.RigidVertices.Num(); ++VertIndex)
				{
					FRigidSkinVertex & Vert = Chunk.RigidVertices[VertIndex];

					uint8 *RemappedBone = BoneMapRemapTable.Find(Vert.Bone);
					if (RemappedBone)
					{
						Vert.Bone = *RemappedBone;
					}
				}

				// fix up soft verts
				for (int32 VertIndex=0; VertIndex < Chunk.SoftVertices.Num(); ++VertIndex)
				{
					FSoftSkinVertex & Vert = Chunk.SoftVertices[VertIndex];
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

			// @todo fix up RequiredBones/ActiveBoneIndices?
			
		}
	}

	void ReduceBoneCounts( USkeletalMesh* SkeletalMesh, int32 DesiredLOD ) override
	{
		check (SkeletalMesh);
		USkeleton* Skeleton = SkeletalMesh->Skeleton;
		check (Skeleton);

		// find all the bones to remove from Skeleton settings
		TMap<FBoneIndexType, FBoneIndexType> BonesToRemove;

		if (GetBoneReductionData(SkeletalMesh, DesiredLOD, BonesToRemove) == false)
		{
			return;
		}

		FStaticLODModel& SrcModel = SkeletalMesh->PreModifyMesh();

		TComponentReregisterContext<USkinnedMeshComponent> ReregisterContext;
		SkeletalMesh->ReleaseResources();
		SkeletalMesh->ReleaseResourcesFence.Wait();

		FSkeletalMeshResource* SkeletalMeshResource = SkeletalMesh->GetImportedResource();
		check(SkeletalMeshResource);
		// Insert a new LOD model entry if needed.
		if ( DesiredLOD == SkeletalMeshResource->LODModels.Num() )
		{
			SkeletalMeshResource->LODModels.Add(0);
		}

		FStaticLODModel** LODModels = SkeletalMeshResource->LODModels.GetData();
		delete LODModels[DesiredLOD];
		FStaticLODModel* NewModel = new FStaticLODModel();
		LODModels[DesiredLOD] = NewModel;

		// Bulk data arrays need to be locked before a copy can be made.
		SrcModel.RawPointIndices.Lock( LOCK_READ_ONLY );
		SrcModel.LegacyRawPointIndices.Lock( LOCK_READ_ONLY );
		*NewModel = SrcModel;
		SrcModel.RawPointIndices.Unlock();
		SrcModel.LegacyRawPointIndices.Unlock();

		// The index buffer needs to be rebuilt on copy.
		FMultiSizeIndexContainerData IndexBufferData;
		SrcModel.MultiSizeIndexContainer.GetIndexBufferData( IndexBufferData );
		NewModel->MultiSizeIndexContainer.RebuildIndexBuffer( IndexBufferData );

		// Required bones are recalculated later on.
		NewModel->RequiredBones.Empty();

		// fix up chunks
		for ( int32 ChunkIndex=0; ChunkIndex< NewModel->Chunks.Num(); ++ChunkIndex )	
		{
			FixUpChunkBoneMaps(NewModel->Chunks[ChunkIndex], BonesToRemove);
		}

		// Copy over LOD info from LOD0 if there is no previous info.
		if ( DesiredLOD == SkeletalMesh->LODInfo.Num() )
		{
			FSkeletalMeshLODInfo* NewLODInfo = new( SkeletalMesh->LODInfo ) FSkeletalMeshLODInfo;
			FSkeletalMeshLODInfo& OldLODInfo = SkeletalMesh->LODInfo[0];
			*NewLODInfo = OldLODInfo;
		}

		SkeletalMesh->CalculateRequiredBones( SkeletalMeshResource->LODModels[DesiredLOD], SkeletalMesh->RefSkeleton, &BonesToRemove );
		SkeletalMesh->PostEditChange();
		SkeletalMesh->InitResources();
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
