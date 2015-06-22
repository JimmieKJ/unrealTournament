// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"

/**
 * Mesh reduction interface.
 */
class IMeshReduction
{
public:
	/**
	 * Reduces the raw mesh using the provided reduction settings.
	 * @param OutReducedMesh - Upon return contains the reduced mesh.
	 * @param OutMaxDeviation - Upon return contains the maximum distance by which the reduced mesh deviates from the original.
	 * @param InMesh - The mesh to reduce.
	 * @param ReductionSettings - Setting with which to reduce the mesh.
	 */
	virtual void Reduce(
		struct FRawMesh& OutReducedMesh,
		float& OutMaxDeviation,
		const struct FRawMesh& InMesh,
		const struct FMeshReductionSettings& ReductionSettings
		) = 0;
	/**
	 * Reduces the provided skeletal mesh.
	 * @returns true if reduction was successful.
	 */
	virtual bool ReduceSkeletalMesh(
		class USkeletalMesh* SkeletalMesh,
		int32 LODIndex,
		const struct FSkeletalMeshOptimizationSettings& Settings,
		bool bCalcLODDistance
		) = 0;
	/**
	 * Returns a unique string identifying both the reduction plugin itself and the version of the plugin.
	 */
	virtual const FString& GetVersionString() const = 0;

	/**
	 *	Returns true if mesh reduction is supported
	 */
	virtual bool IsSupported() const = 0;
};

//
namespace MaterialExportUtils
{
	struct FFlattenMaterial;
};

/**
 * Mesh merging interface.
 */
class IMeshMerging
{
public:
	virtual void BuildProxy(
		const TArray<FRawMesh>& InputMeshes,
		const TArray<MaterialExportUtils::FFlattenMaterial>& InputMaterials,
		const struct FMeshProxySettings& InProxySettings,
		FRawMesh& OutProxyMesh,
		MaterialExportUtils::FFlattenMaterial& OutMaterial
		) = 0;
};


/**
 * Mesh reduction module interface.
 */
class IMeshReductionModule : public IModuleInterface
{
public:
	/**
	 * Retrieve the mesh reduction interface.
	 */
	virtual class IMeshReduction* GetMeshReductionInterface() = 0;
	
	/**
	 * Retrieve the mesh merging interface.
	 */
	virtual class IMeshMerging* GetMeshMergingInterface() = 0;
};

class IMeshUtilities : public IModuleInterface
{
public:
	/** Returns a string uniquely identifying this version of mesh utilities. */
	virtual const FString& GetVersionString() const = 0;

	/**
	 * Builds a renderable static mesh using the provided source models and the LOD groups settings.
	 * @returns true if the renderable mesh was built successfully.
	 */
	virtual bool BuildStaticMesh(
		class FStaticMeshRenderData& OutRenderData,
		TArray<struct FStaticMeshSourceModel>& SourceModels,
		const class FStaticMeshLODGroup& LODGroup
		) = 0;

	/**
	 * Builds a static mesh using the provided source models and the LOD groups settings, and replaces
	 * the RawMeshes with the reduced meshes. Does not modify renderable data.
	 * @returns true if the meshes were built successfully.
	 */
	virtual bool GenerateStaticMeshLODs(
		TArray<struct FStaticMeshSourceModel>& Models,
		const class FStaticMeshLODGroup& LODGroup
		) = 0;

	/** Builds a signed distance field volume for the given LODModel. */
	virtual void GenerateSignedDistanceFieldVolumeData(
		const FStaticMeshLODResources& LODModel,
		class FQueuedThreadPool& ThreadPool,
		const TArray<EBlendMode>& MaterialBlendModes,
		const FBoxSphereBounds& Bounds,
		float DistanceFieldResolutionScale,
		bool bGenerateAsIfTwoSided,
		class FDistanceFieldVolumeData& OutData) = 0;

	/**
	 * Create all render specific data for a skeletal mesh LOD model
	 * @returns true if the mesh was built successfully.
	 */
	virtual bool BuildSkeletalMesh( 
		FStaticLODModel& LODModel,
		const FReferenceSkeleton& RefSkeleton,
		const TArray<FVertInfluence>& Influences, 
		const TArray<FMeshWedge>& Wedges, 
		const TArray<FMeshFace>& Faces, 
		const TArray<FVector>& Points,
		const TArray<int32>& PointToOriginalMap,
		bool bKeepOverlappingVertices = false,
		bool bComputeNormals = true,
		bool bComputeTangents = true,
		TArray<FText> * OutWarningMessages = NULL,
		TArray<FName> * OutWarningNames = NULL
		) = 0;

	/** Returns the mesh reduction plugin if available. */
	virtual IMeshReduction* GetMeshReductionInterface() = 0;

	/** Returns the mesh merging plugin if available. */
	virtual IMeshMerging* GetMeshMergingInterface() = 0;

	/** Cache optimize the index buffer. */
	virtual void CacheOptimizeIndexBuffer(TArray<uint16>& Indices) = 0;

	/** Cache optimize the index buffer. */
	virtual void CacheOptimizeIndexBuffer(TArray<uint32>& Indices) = 0;

	/** Build adjacency information for the skeletal mesh used for tessellation. */
	virtual void BuildSkeletalAdjacencyIndexBuffer(
		const TArray<struct FSoftSkinVertex>& VertexBuffer,
		const uint32 TexCoordCount,
		const TArray<uint32>& Indices,
		TArray<uint32>& OutPnAenIndices
		) = 0;

	
	virtual void RechunkSkeletalMeshModels(USkeletalMesh* SrcMesh, int32 MaxBonesPerChunk) = 0;

	/**
	 *	Calculate the verts associated weighted to each bone of the skeleton.
	 *	The vertices returned are in the local space of the bone.
	 *
	 *	@param	SkeletalMesh	The target skeletal mesh.
	 *	@param	Infos			The output array of vertices associated with each bone.
	 *	@param	bOnlyDominant	Controls whether a vertex is added to the info for a bone if it is most controlled by that bone, or if that bone has ANY influence on that vert.
	 */
	virtual void CalcBoneVertInfos( USkeletalMesh* SkeletalMesh, TArray<FBoneVertInfo>& Infos, bool bOnlyDominant) = 0;

	/**
	 * Harvest static mesh components from input actors 
	 * and merge into signle mesh grouping them by unique materials
	 *
	 * @param SourceActors				List of actors to merge
	 * @param InSettings				Settings to use
	 * @param InOuter					Outer if required
	 * @param InBasePackageName			Destination package name for a generated assets. Used if Outer is null. 
	 * @param UseLOD					-1 if you'd like to build for all LODs. If you specify, that LOD mesh for source meshes will be used to merge the mesh
	 *									This is used by hierarchical building LODs
	 * @param OutAssetsToSync			Merged mesh assets
	 * @param OutMergedActorLocation	World position of merged mesh
	 */
	virtual void MergeActors(
		const TArray<AActor*>& SourceActors,
		const FMeshMergingSettings& InSettings,
		UPackage* InOuter,
		const FString& InBasePackageName,
		int32 UseLOD, // does not build all LODs but only use this LOD to create base mesh
		TArray<UObject*>& OutAssetsToSync, 
		FVector& OutMergedActorLocation, 
		bool bSilent=false) const = 0;
	
	/**
	 *	Merges list of actors into single proxy mesh
	 *
	 *	@param	Actors					List of Actors to merge
	 *	@param	InProxySettings			Merge settings
	 *	@param	InOuter					Package for a generated assets, if NULL new packages will be created for each asset
	 *	@param	ProxyBasePackageName	Will be used for naming generated assets, in case InOuter is not specified ProxyBasePackageName will be used as long package name for creating new packages
	 *	@param	OutAssetsToSync			Result assets - mesh, material
	 *	@param	OutProxyLocation		Proxy mesh location in the world (bounding box origin of merged actors)
	 */
	virtual void CreateProxyMesh(
		const TArray<AActor*>& Actors,
		const struct FMeshProxySettings& InProxySettings,
		UPackage* InOuter,
		const FString& ProxyBasePackageName,
		TArray<UObject*>& OutAssetsToSync,
		FVector& OutProxyLocation
		) = 0;
};
