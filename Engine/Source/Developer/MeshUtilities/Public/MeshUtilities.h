// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "SkeletalMeshTypes.h"
#include "Engine/MeshMerging.h"

typedef FIntPoint FMeshIdAndLOD;
struct FFlattenMaterial;
struct FReferenceSkeleton;
struct FStaticMeshLODResources;

namespace ETangentOptions
{
	enum Type
	{
		None = 0,
		BlendOverlappingNormals = 0x1,
		IgnoreDegenerateTriangles = 0x2,
	};
};

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
		bool bCalcLODDistance,
		bool bReregisterComponent = true
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

DECLARE_DELEGATE_ThreeParams(FProxyCompleteDelegate, struct FRawMesh&, struct FFlattenMaterial&, const FGuid);
DECLARE_DELEGATE_TwoParams(FProxyFailedDelegate, const FGuid, const FString&);
DECLARE_DELEGATE_TwoParams(FCreateProxyDelegate, const FGuid, TArray<UObject*>&);

/** Data used for passing back the data resulting from a completed mesh merging operation*/
struct FMergeCompleteData
{
	/** Outer object for the to store/save UObjects */
	class UPackage* InOuter;
	/** Base package name for the proxy mesh UObjects */
	FString ProxyBasePackageName;
	/** Merge/Proxy settings structure */
	struct FMeshProxySettings InProxySettings;
	/** Callback delegate object used as a callback when the job finishes */
	FCreateProxyDelegate CallbackDelegate;
};

/**
 * Mesh merging interface.
 */
class IMeshMerging
{
public:
	virtual void ProxyLOD(const TArray<struct FMeshMergeData>& InData,
		const struct FMeshProxySettings& InProxySettings,
		const TArray<struct FFlattenMaterial>& InputMaterials,
		const FGuid InJobGUID) {}

	virtual void AggregateLOD() {}

	FProxyCompleteDelegate CompleteDelegate;
	FProxyFailedDelegate FailedDelegate;
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

	/** Helper structure for skeletal mesh import options */
	struct MeshBuildOptions
	{
		MeshBuildOptions()
		: bKeepOverlappingVertices(false)
		, bRemoveDegenerateTriangles(false)
		, bComputeNormals(true)
		, bComputeTangents(true)
		, bUseMikkTSpace(false)
		{
		}

		bool bKeepOverlappingVertices;
		bool bRemoveDegenerateTriangles;
		bool bComputeNormals;
		bool bComputeTangents;
		bool bUseMikkTSpace;
	};
	
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
		const MeshBuildOptions& BuildOptions = MeshBuildOptions(),
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
		TArray<UObject*>& OutAssetsToSync, 
		FVector& OutMergedActorLocation, 
		bool bSilent=false) const = 0;

	virtual void MergeActors(
		const TArray<AActor*>& SourceActors,
		const FMeshMergingSettings& InSettings,
		UPackage* InOuter,		
		const FString& InBasePackageName,
		int32 UseLOD, // does not build all LODs but only use this LOD to create base mesh
		TArray<UObject*>& OutAssetsToSync,
		FVector& OutMergedActorLocation,
		bool bSilent = false) const = 0;


	/**
	* MergeStaticMeshComponents
	*
	* @param ComponentsToMerge - Components to merge
	* @param World - World in which the component reside
	* @param InSettings	- Settings to use
	* @param InOuter - Outer if required
	* @param InBasePackageName - Destination package name for a generated assets. Used if Outer is null.
	* @param UseLOD	-1 if you'd like to build for all LODs. If you specify, that LOD mesh for source meshes will be used to merge the mesh
	*									This is used by hierarchical building LODs
	* @param OutAssetsToSync Merged mesh assets
	* @param OutMergedActorLocation	World position of merged mesh
	* @param ViewDistance Distance for LOD determination
	* @param bSilent Non-verbose flag
	* @return void
	*/
	virtual void MergeStaticMeshComponents(
		const TArray<UStaticMeshComponent*>& ComponentsToMerge,
		UWorld* World,
		const FMeshMergingSettings& InSettings,
		UPackage* InOuter,
		const FString& InBasePackageName,
		TArray<UObject*>& OutAssetsToSync,
		FVector& OutMergedActorLocation,
		const float ScreenAreaSize,
		bool bSilent /*= false*/) const = 0;

	virtual void MergeStaticMeshComponents(
		const TArray<UStaticMeshComponent*>& ComponentsToMerge,
		UWorld* World,
		const FMeshMergingSettings& InSettings,
		UPackage* InOuter,
		const FString& InBasePackageName,
		int32 UseLOD, // does not build all LODs but only use this LOD to create base mesh
		TArray<UObject*>& OutAssetsToSync,
		FVector& OutMergedActorLocation,
		const float ScreenAreaSize,
		bool bSilent = false) const = 0;

	/**
	* Creates a (proxy)-mesh combining the static mesh components from the given list of actors (at the moment this requires having Simplygon)
	*
	* @param InActors - List of Actors to merge
	* @param InMeshProxySettings - Merge settings
	* @param InOuter - Package for a generated assets, if NULL new packages will be created for each asset
	* @param InProxyBasePackageName - Will be used for naming generated assets, in case InOuter is not specified ProxyBasePackageName will be used as long package name for creating new packages
	* @param InGuid - Guid identifying the data used for this proxy job
	* @param InProxyCreatedDelegate - Delegate callback for when the proxy is finished
	* @param bAllowAsync - Flag whether or not this call could be run async (SimplygonSwarm)
	*/
	virtual void CreateProxyMesh(const TArray<class AActor*>& InActors, const struct FMeshProxySettings& InMeshProxySettings, UPackage* InOuter, const FString& InProxyBasePackageName, const FGuid InGuid, FCreateProxyDelegate InProxyCreatedDelegate, const bool bAllowAsync = false, const float ScreenAreaSize = 1.0f) = 0;

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

	/**
	*	Merges list of actors into single proxy mesh
	*
	*	@param	Actors					List of Actors to merge
	*	@param	InProxySettings			Merge settings
	*	@param	InOuter					Package for a generated assets, if NULL new packages will be created for each asset
	*	@param	ProxyBasePackageName	Will be used for naming generated assets, in case InOuter is not specified ProxyBasePackageName will be used as long package name for creating new packages
	*	@param	OutAssetsToSync			Result assets - mesh, material
	*/
	virtual void CreateProxyMesh(
		const TArray<AActor*>& Actors,
		const struct FMeshProxySettings& InProxySettings,
		UPackage* InOuter,
		const FString& ProxyBasePackageName,
		TArray<UObject*>& OutAssetsToSync, 		
		const float ScreenAreaSize = 1.0f) = 0;

	/**
	* FlattenMaterialsWithMeshData
	*
	* @param InMaterials - List of unique materials used by InSourceMeshes
	* @param InSourceMeshes - List of raw meshes used to flatten the materials with (vertex data)
	* @param InMaterialIndexMap - Map used for mapping the raw meshes to the correct materials	
	* @param InMeshShouldBakeVertexData - Array of flags to determine whether or not a mesh requires to have its vertex data baked down
	* @param InMaterialProxySettings - Settings for creating the flattened material
	* @param OutFlattenedMaterials - List of flattened materials (one for each mesh)
	*/
	virtual	void FlattenMaterialsWithMeshData(TArray<UMaterialInterface*>& InMaterials, TArray<struct FRawMeshExt>& InSourceMeshes, TMap<FMeshIdAndLOD, TArray<int32>>& InMaterialIndexMap, TArray<bool>& InMeshShouldBakeVertexData, const FMaterialProxySettings &InMaterialProxySettings, TArray<FFlattenMaterial> &OutFlattenedMaterials) const = 0;

	/**
	* Propagates vertex painted colors from the StaticMeshComponent instance to RawMesh
	*
	* @param StaticMeshComponent - Instance of the StaticMesh
	* @param LODIndex - LOD index to propagate the vertex colors from
	* @param RawMesh - Target raw mesh to propagate vertex colours to
	* @return bool - whether or not propagating succeeded
	*/
	virtual bool PropagatePaintedColorsToRawMesh(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FRawMesh& RawMesh) const = 0;
	
	/**
	*	Calculates UV coordinates bounds for the given Raw Mesh
	*
	* @param InRawMesh - Raw mesh to calculate the bounds for
	* @param OutBounds - Out texture bounds (min-max)
	*/
	virtual void CalculateTextureCoordinateBoundsForRawMesh(const FRawMesh& InRawMesh, TArray<FBox2D>& OutBounds) const = 0;

	/**
	*	Calculates UV coordinates bounds for the given Skeletal Mesh
	*
	* @param InRawMesh - Skeletal Mesh to calculate the bounds for
	* @param OutBounds - Out texture bounds (min-max)
	*/
	virtual void CalculateTextureCoordinateBoundsForSkeletalMesh(const FStaticLODModel& LODModel, TArray<FBox2D>& OutBounds) const = 0;

	/**
	* Calculates (new) non-overlapping UV coordinates for the given Raw Mesh
	*
	* @param RawMesh - Raw Mesh to generate UV coordinates for
	* @param TextureResolution - Texture resolution to take into account while generating the UVs
	* @param OutTexCoords - New set of UV coordinates
	* @return bool - whether or not generating the UVs succeeded
	*/
	virtual bool GenerateUniqueUVsForStaticMesh(const FRawMesh& RawMesh, int32 TextureResolution, TArray<FVector2D>& OutTexCoords) const = 0;


	/** Calculates (new) non-overlapping UV coordinates for the given Skeletal Mesh
	*
	* @param LODModel - Skeletal Mesh to generate UV coordinates for
	* @param TextureResolution - Texture resolution to take into account while generating the UVs
	* @param OutTexCoords - New set of UV coordinates
	* @return bool - whether or not generating the UVs succeeded
	*/
	virtual bool GenerateUniqueUVsForSkeletalMesh(const FStaticLODModel& LODModel, int32 TextureResolution, TArray<FVector2D>& OutTexCoords) const = 0;	

	/**
	 * Remove Bones based on LODInfo setting
	 *
	 * @param SkeletalMesh	Mesh that needs bones to be removed
	 * @param LODIndex		Desired LOD to remove bones [ 0 based ]
	 * @param BoneNamesToRemove	List of bone names to remove
	 *
	 * @return true if success
	 */
	virtual bool RemoveBonesFromMesh(USkeletalMesh* SkeletalMesh, int32 LODIndex, const TArray<FName>* BoneNamesToRemove) const = 0;

	/** 
	 * Calculates Tangents and Normals for a given set of vertex data
	 * 
	 * @param InVertices Vertices that make up the mesh
	 * @param InIndices Indices for the Vertex array
	 * @param InUVs Texture coordinates (per-index based)
	 * @param InSmoothingGroupIndices Smoothing group index (per-face based)
	 * @param InTangentOptions Flags for Tangent calculation
	 * @param OutTangentX Array to hold calculated Tangents
	 * @param OutTangentX Array to hold calculated Tangents
	 * @param OutNormals Array to hold calculated normals (if already contains normals will use those instead for the tangent calculation	
	*/
	virtual void CalculateTangents(const TArray<FVector>& InVertices, const TArray<uint32>& InIndices, const TArray<FVector2D>& InUVs, const TArray<uint32>& InSmoothingGroupIndices, const uint32 InTangentOptions, TArray<FVector>& OutTangentX, TArray<FVector>& OutTangentY, TArray<FVector>& OutNormals) const = 0;
};
