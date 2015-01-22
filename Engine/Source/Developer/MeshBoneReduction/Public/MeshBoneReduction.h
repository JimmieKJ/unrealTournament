// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"

/**
 * Mesh reduction interface.
 */
class IMeshBoneReduction
{
public:
	/**
	 * Fix up chunk bone maps based on list of bones to remove
	 * List of bones to remove should contains <bone index to remove, bone index to replace to >
	 * 
	 * @param	Chunk : Chunk reference to fix up
	 * @param	BonesToRemove : List of bones to remove with a pair of [bone index, bone to replace]
	 *
	 * @return	true if success 
	 */
	virtual void FixUpChunkBoneMaps( FSkelMeshChunk & Chunk, const TMap<FBoneIndexType, FBoneIndexType> &BonesToRepair ) = 0;
	/**
	 * Get Bones To Remove from the Desired LOD
	 * List of bones to remove should contains <bone index to remove, bone index to replace to >
	 *
	 * @param	SkeletalMesh : SkeletalMesh to test
	 * @param	DesiredLOD	: 0 isn't valid as this will only test from [LOD 1, LOD (N-1)] since Skeleton doesn't save any bones to remove setting on based LOD
	 * @param	OutBonesToReplace : List of bones to replace with a pair of [bone index, bone index to replace to]
	 *
	 * @return	true if any bone to be replaced
	 */
	virtual bool GetBoneReductionData( const USkeletalMesh* SkeletalMesh, int32 DesiredLOD, TMap<FBoneIndexType, FBoneIndexType> &OutBonesToReplace ) = 0;

	/**
	 * Reduce Bone Counts for the SkeletalMesh with the LOD
	 *
	 * @param	SkeletalMesh : SkeletalMesh
	 * @param	DesriedLOD	: The data to reduce comes from Skeleton 
	 *
	 */
	virtual void ReduceBoneCounts( USkeletalMesh * SkeletalMesh, int32 DesiredLOD) = 0;
};

/**
 * Mesh reduction module interface.
 */
class IMeshBoneReductionModule : public IModuleInterface
{
public:
	/**
	 * Retrieve the mesh reduction interface.
	 */
	virtual class IMeshBoneReduction* GetMeshBoneReductionInterface() = 0;
};
