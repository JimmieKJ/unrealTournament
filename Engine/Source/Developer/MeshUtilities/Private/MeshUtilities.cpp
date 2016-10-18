// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MeshUtilitiesPrivate.h"
#include "StaticMeshResources.h"
#include "SkeletalMeshTypes.h"
#include "MeshBuild.h"
#include "TessellationRendering.h"
#include "NvTriStrip.h"
#include "forsythtriangleorderoptimizer.h"
#include "nvtess.h"
#include "SkeletalMeshTools.h"
#include "ImageUtils.h"
#include "Textures/TextureAtlas.h"
#include "LayoutUV.h"
#include "mikktspace.h"
#include "DistanceFieldAtlas.h"
#include "FbxErrors.h"
#include "Components/SplineMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "MaterialUtilities.h"
#include "HierarchicalLODUtilities.h"
#include "HierarchicalLODUtilitiesModule.h"
#include "MeshBoneReduction.h"
#include "MeshMergeData.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "GPUSkinVertexFactory.h"

#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "Engine/HLODMeshCullingVolume.h"
#include "ProxyMaterialUtilities.h"

//@todo - implement required vector intrinsics for other implementations
#if PLATFORM_ENABLE_VECTORINTRINSICS
#include "kDOP.h"
#endif

#if WITH_EDITOR
#include "Editor.h"
#endif

/*------------------------------------------------------------------------------
MeshUtilities module.
------------------------------------------------------------------------------*/

// The version string is a GUID. If you make a change to mesh utilities that
// causes meshes to be rebuilt you MUST generate a new GUID and replace this
// string with it.

#define MESH_UTILITIES_VER TEXT("8C68575CEF434CA8A9E1DA4AED8A47BB")

DEFINE_LOG_CATEGORY_STATIC(LogMeshUtilities, Verbose, All);

#define LOCTEXT_NAMESPACE "MeshUtils"

// CVars
static TAutoConsoleVariable<int32> CVarTriangleOrderOptimization(
	TEXT("r.TriangleOrderOptimization"),
	1,
	TEXT("Controls the algorithm to use when optimizing the triangle order for the post-transform cache.\n")
	TEXT("0: Use NVTriStrip (slower)\n")
	TEXT("1: Use Forsyth algorithm (fastest)(default)")
	TEXT("2: No triangle order optimization. (least efficient, debugging purposes only)"),
	ECVF_Default);

class FMeshUtilities : public IMeshUtilities
{
public:
	/** Default constructor. */
	FMeshUtilities()
		: MeshReduction(NULL)
		, MeshMerging(NULL)
		, DistributedMeshMerging(NULL)
		, Processor(NULL)
	{
	}

private:
	/** Cached pointer to the mesh reduction interface. */
	IMeshReduction* MeshReduction;
	/** Cached pointer to the mesh merging interface. */
	IMeshMerging* MeshMerging;
	/** Cached pointer to the distributed mesh merging interface. */
	IMeshMerging* DistributedMeshMerging;
	/** Cached version string. */
	FString VersionString;
	/** True if Simplygon is being used for mesh reduction. */
	bool bUsingSimplygon;
	/** True if NvTriStrip is being used for tri order optimization. */
	bool bUsingNvTriStrip;
	/** True if we disable triangle order optimization.  For debugging purposes only */
	bool bDisableTriangleOrderOptimization;

	class FProxyGenerationProcessor* Processor;

	// IMeshUtilities interface.
	virtual const FString& GetVersionString() const override
	{
		return VersionString;
	}

	virtual bool BuildStaticMesh(
		FStaticMeshRenderData& OutRenderData,
		TArray<FStaticMeshSourceModel>& SourceModels,
		const FStaticMeshLODGroup& LODGroup
		) override;

	virtual bool GenerateStaticMeshLODs(TArray<FStaticMeshSourceModel>& Models, const FStaticMeshLODGroup& LODGroup) override;

	virtual void GenerateSignedDistanceFieldVolumeData(
		const FStaticMeshLODResources& LODModel,
	class FQueuedThreadPool& ThreadPool,
		const TArray<EBlendMode>& MaterialBlendModes,
		const FBoxSphereBounds& Bounds,
		float DistanceFieldResolutionScale,
		bool bGenerateAsIfTwoSided,
		FDistanceFieldVolumeData& OutData) override;

	virtual bool BuildSkeletalMesh(FStaticLODModel& LODModel, const FReferenceSkeleton& RefSkeleton, const TArray<FVertInfluence>& Influences, const TArray<FMeshWedge>& Wedges, const TArray<FMeshFace>& Faces, const TArray<FVector>& Points, const TArray<int32>& PointToOriginalMap, const MeshBuildOptions& BuildOptions = MeshBuildOptions(), TArray<FText> * OutWarningMessages = NULL, TArray<FName> * OutWarningNames = NULL) override;
	bool BuildSkeletalMesh_Legacy(FStaticLODModel& LODModel, const FReferenceSkeleton& RefSkeleton, const TArray<FVertInfluence>& Influences, const TArray<FMeshWedge>& Wedges, const TArray<FMeshFace>& Faces, const TArray<FVector>& Points, const TArray<int32>& PointToOriginalMap, bool bKeepOverlappingVertices = false, bool bComputeNormals = true, bool bComputeTangents = true, TArray<FText> * OutWarningMessages = NULL, TArray<FName> * OutWarningNames = NULL);

	virtual IMeshReduction* GetMeshReductionInterface() override;
	virtual IMeshMerging* GetMeshMergingInterface() override;
	virtual void CacheOptimizeIndexBuffer(TArray<uint16>& Indices) override;
	virtual void CacheOptimizeIndexBuffer(TArray<uint32>& Indices) override;
	void CacheOptimizeVertexAndIndexBuffer(TArray<FStaticMeshBuildVertex>& Vertices, TArray<TArray<uint32> >& PerSectionIndices, TArray<int32>& WedgeMap);

	virtual void BuildSkeletalAdjacencyIndexBuffer(
		const TArray<FSoftSkinVertex>& VertexBuffer,
		const uint32 TexCoordCount,
		const TArray<uint32>& Indices,
		TArray<uint32>& OutPnAenIndices
		) override;

	virtual void RechunkSkeletalMeshModels(USkeletalMesh* SrcMesh, int32 MaxBonesPerChunk) override;

	virtual void CalcBoneVertInfos(USkeletalMesh* SkeletalMesh, TArray<FBoneVertInfo>& Infos, bool bOnlyDominant) override;

	/**
	* Builds a renderable skeletal mesh LOD model. Note that the array of chunks
	* will be destroyed during this process!
	* @param LODModel				Upon return contains a renderable skeletal mesh LOD model.
	* @param RefSkeleton			The reference skeleton associated with the model.
	* @param Chunks				Skinned mesh chunks from which to build the renderable model.
	* @param PointToOriginalMap	Maps a vertex's RawPointIdx to its index at import time.
	*/
	void BuildSkeletalModelFromChunks(FStaticLODModel& LODModel, const FReferenceSkeleton& RefSkeleton, TArray<FSkinnedMeshChunk*>& Chunks, const TArray<int32>& PointToOriginalMap);

	// IModuleInterface interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	DEPRECATED(4.12, "Please use MergeActor with new signature instead")
	virtual void MergeActors(
		const TArray<AActor*>& SourceActors,
		const FMeshMergingSettings& InSettings,
		UPackage* InOuter,
		const FString& InBasePackageName,
		int32 UseLOD, // does not build all LODs but only use this LOD to create base mesh
		TArray<UObject*>& OutAssetsToSync,
		FVector& OutMergedActorLocation,
		bool bSilent = false) const override;

	virtual void MergeActors(
		const TArray<AActor*>& SourceActors,
		const FMeshMergingSettings& InSettings,
		UPackage* InOuter,
		const FString& InBasePackageName,
		TArray<UObject*>& OutAssetsToSync,
		FVector& OutMergedActorLocation,
		bool bSilent = false) const override;


	DEPRECATED(4.12, "Please use MergeStaticMeshComponents with new signature instead") 
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
		bool bSilent = false) const override;

	virtual void MergeStaticMeshComponents(
		const TArray<UStaticMeshComponent*>& ComponentsToMerge,
		UWorld* World,
		const FMeshMergingSettings& InSettings,
		UPackage* InOuter,
		const FString& InBasePackageName,
		TArray<UObject*>& OutAssetsToSync,
		FVector& OutMergedActorLocation,
		const float ScreenAreaSize,
		bool bSilent = false) const override;

	virtual void CreateProxyMesh(const TArray<AActor*>& InActors, const struct FMeshProxySettings& InMeshProxySettings, UPackage* InOuter, const FString& InProxyBasePackageName, const FGuid InGuid, FCreateProxyDelegate InProxyCreatedDelegate, const bool bAllowAsync,
		const float ScreenAreaSize = 1.0f) override;

	DEPRECATED(4.11, "Please use CreateProxyMesh with new signature")
		virtual void CreateProxyMesh(
		const TArray<AActor*>& Actors,
		const struct FMeshProxySettings& InProxySettings,
		UPackage* InOuter,
		const FString& ProxyBasePackageName,
		TArray<UObject*>& OutAssetsToSync,
		FVector& OutProxyLocation
		) override;

	virtual void CreateProxyMesh(
		const TArray<AActor*>& Actors,
		const struct FMeshProxySettings& InProxySettings,
		UPackage* InOuter,
		const FString& ProxyBasePackageName,
		TArray<UObject*>& OutAssetsToSync,
		const float ScreenAreaSize = 1.0f) override;

	virtual void FlattenMaterialsWithMeshData(TArray<UMaterialInterface*>& InMaterials, TArray<FRawMeshExt>& InSourceMeshes, TMap<FMeshIdAndLOD, TArray<int32>>& InMaterialIndexMap, TArray<bool>& InMeshShouldBakeVertexData, const FMaterialProxySettings &InMaterialProxySettings, TArray<FFlattenMaterial> &OutFlattenedMaterials) const override;

	bool ConstructRawMesh(
		const UStaticMeshComponent* InMeshComponent,
		int32 InLODIndex,
		const bool bPropagateVertexColours,
		FRawMesh& OutRawMesh,
		TArray<UMaterialInterface*>& OutUniqueMaterials,
		TArray<int32>& OutGlobalMaterialIndices
		) const;

	virtual void ExtractMeshDataForGeometryCache(FRawMesh& RawMesh, const FMeshBuildSettings& BuildSettings, TArray<FStaticMeshBuildVertex>& OutVertices, TArray<TArray<uint32> >& OutPerSectionIndices);

	virtual bool PropagatePaintedColorsToRawMesh(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FRawMesh& RawMesh) const override;

	virtual void CalculateTextureCoordinateBoundsForRawMesh(const FRawMesh& InRawMesh, TArray<FBox2D>& OutBounds) const override;

	virtual void CalculateTextureCoordinateBoundsForSkeletalMesh(const FStaticLODModel& LODModel, TArray<FBox2D>& OutBounds) const override;

	virtual bool GenerateUniqueUVsForStaticMesh(const FRawMesh& RawMesh, int32 TextureResolution, TArray<FVector2D>& OutTexCoords) const override;
	virtual bool GenerateUniqueUVsForSkeletalMesh(const FStaticLODModel& LODModel, int32 TextureResolution, TArray<FVector2D>& OutTexCoords) const override;

	virtual bool RemoveBonesFromMesh(USkeletalMesh* SkeletalMesh, int32 LODIndex, const TArray<FName>* BoneNamesToRemove) const override;

	virtual void CalculateTangents(const TArray<FVector>& InVertices, const TArray<uint32>& InIndices, const TArray<FVector2D>& InUVs, const TArray<uint32>& InSmoothingGroupIndices, const uint32 InTangentOptions, TArray<FVector>& OutTangentX, TArray<FVector>& OutTangentY, TArray<FVector>& OutNormals) const override;

	// Need to call some members from this class, (which is internal to this module)
	friend class FStaticMeshUtilityBuilder;
};

IMPLEMENT_MODULE(FMeshUtilities, MeshUtilities);

class FProxyGenerationProcessor : FTickerObjectBase
{
public:
	FProxyGenerationProcessor()
	{
#if WITH_EDITOR
		FEditorDelegates::MapChange.AddRaw(this, &FProxyGenerationProcessor::OnMapChange);
		FEditorDelegates::NewCurrentLevel.AddRaw(this, &FProxyGenerationProcessor::OnNewCurrentLevel);
#endif // WITH_EDITOR
	}

	~FProxyGenerationProcessor()
	{
#if WITH_EDITOR
		FEditorDelegates::MapChange.RemoveAll(this);
		FEditorDelegates::NewCurrentLevel.RemoveAll(this);
#endif // WITH_EDITOR
	}

	void AddProxyJob(FGuid InJobGuid, FMergeCompleteData* InCompleteData)
	{
		FScopeLock Lock(&StateLock);
		ProxyMeshJobs.Add(InJobGuid, InCompleteData);
	}

	virtual bool Tick(float DeltaTime) override
	{
		FScopeLock Lock(&StateLock);
		for (const auto& Entry : ToProcessJobDataMap)
		{
			FGuid JobGuid = Entry.Key;
			FProxyGenerationData* Data = Entry.Value;

			// Process the job
			ProcessJob(JobGuid, Data);

			// Data retrieved so can now remove the job from the map
			ProxyMeshJobs.Remove(JobGuid);
			delete Data->MergeData;
			delete Data;
		}

		ToProcessJobDataMap.Reset();

		return true;
	}

	void ProxyGenerationComplete(FRawMesh& OutProxyMesh, struct FFlattenMaterial& OutMaterial, const FGuid OutJobGUID)
	{
		FScopeLock Lock(&StateLock);
		FMergeCompleteData** FindData = ProxyMeshJobs.Find(OutJobGUID);
		if (FindData && *FindData)
		{
			FMergeCompleteData* Data = *FindData;

			FProxyGenerationData* GenerationData = new FProxyGenerationData();
			GenerationData->Material = OutMaterial;
			GenerationData->RawMesh = OutProxyMesh;
			GenerationData->MergeData = Data;

			ToProcessJobDataMap.Add(OutJobGUID, GenerationData);
		}
	}

	void ProxyGenerationFailed(const FGuid OutJobGUID, const FString& ErrorMessage )
	{
		FScopeLock Lock(&StateLock);
		FMergeCompleteData** FindData = ProxyMeshJobs.Find(OutJobGUID);
		if (FindData)
		{
			ProxyMeshJobs.Remove(OutJobGUID);
			if (*FindData)
			{
				UE_LOG(LogMeshUtilities, Log, TEXT("Failed to generate proxy mesh for cluster %s, %s"), *(*FindData)->ProxyBasePackageName, *ErrorMessage);
			}
		}
	}

protected:
	/** Called when the map has changed*/
	void OnMapChange(uint32 MapFlags)
	{
		ClearProcessingData();
	}

	/** Called when the current level has changed */
	void OnNewCurrentLevel()
	{
		ClearProcessingData();
	}

	/** Clears the processing data array/map */
	void ClearProcessingData()
	{
		FScopeLock Lock(&StateLock);
		ProxyMeshJobs.Empty();
		ToProcessJobDataMap.Empty();
	}

protected:
	/** Structure storing the data required during processing */
	struct FProxyGenerationData
	{
		FRawMesh RawMesh;
		FFlattenMaterial Material;
		FMergeCompleteData* MergeData;
	};

	void ProcessJob(const FGuid& JobGuid, FProxyGenerationData* Data)
	{	
		TArray<UObject*> OutAssetsToSync;
		const FString AssetBaseName = FPackageName::GetShortName(Data->MergeData->ProxyBasePackageName);
		const FString AssetBasePath = Data->MergeData->InOuter ? TEXT("") : FPackageName::GetLongPackagePath(Data->MergeData->ProxyBasePackageName) + TEXT("/");

		// Retrieve flattened material data
		FFlattenMaterial& FlattenMaterial = Data->Material;
		
		// Resize flattened material
		FMaterialUtilities::ResizeFlattenMaterial(FlattenMaterial, Data->MergeData->InProxySettings);

		// Optimize flattened material
		FMaterialUtilities::OptimizeFlattenMaterial(FlattenMaterial);

		// Create a new proxy material instance
		UMaterialInstanceConstant* ProxyMaterial = ProxyMaterialUtilities::CreateProxyMaterialInstance(Data->MergeData->InOuter, Data->MergeData->InProxySettings.MaterialSettings, FlattenMaterial, AssetBasePath, AssetBaseName);

		// Set material static lighting usage flag if project has static lighting enabled
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnGameThread() != 0);
		if (bAllowStaticLighting)
		{
			ProxyMaterial->CheckMaterialUsage(MATUSAGE_StaticLighting);
		}

		// Construct proxy static mesh
		UPackage* MeshPackage = Data->MergeData->InOuter;
		FString MeshAssetName = TEXT("SM_") + AssetBaseName;
		if (MeshPackage == nullptr)
		{
			MeshPackage = CreatePackage(NULL, *(AssetBasePath + MeshAssetName));
			MeshPackage->FullyLoad();
			MeshPackage->Modify();
		}

		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(MeshPackage, FName(*MeshAssetName), RF_Public | RF_Standalone);
		StaticMesh->InitResources();

		FString OutputPath = StaticMesh->GetPathName();

		// make sure it has a new lighting guid
		StaticMesh->LightingGuid = FGuid::NewGuid();

		// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
		StaticMesh->LightMapResolution = Data->MergeData->InProxySettings.LightMapResolution;
		StaticMesh->LightMapCoordinateIndex = 1;

		FStaticMeshSourceModel* SrcModel = new (StaticMesh->SourceModels) FStaticMeshSourceModel();
		/*Don't allow the engine to recalculate normals*/
		SrcModel->BuildSettings.bRecomputeNormals = false;
		SrcModel->BuildSettings.bRecomputeTangents = false;
		SrcModel->BuildSettings.bRemoveDegenerates = false;
		SrcModel->BuildSettings.bUseHighPrecisionTangentBasis = false;
		SrcModel->BuildSettings.bUseFullPrecisionUVs = false;
		SrcModel->RawMeshBulkData->SaveRawMesh(Data->RawMesh);

		//Assign the proxy material to the static mesh
		StaticMesh->Materials.Add(ProxyMaterial);

		StaticMesh->Build();
		StaticMesh->PostEditChange();

		OutAssetsToSync.Add(StaticMesh);

		// Execute the delegate received from the user
		Data->MergeData->CallbackDelegate.ExecuteIfBound(JobGuid, OutAssetsToSync);
	}
protected:
	/** Holds Proxy mesh job data together with the job Guid */
	TMap<FGuid, FMergeCompleteData*> ProxyMeshJobs;
	/** Holds Proxy generation data together with the job Guid */
	TMap<FGuid, FProxyGenerationData*> ToProcessJobDataMap;
	/** Critical section to keep ProxyMeshJobs/ToProcessJobDataMap access thread-safe */
	FCriticalSection StateLock;
};

//@todo - implement required vector intrinsics for other implementations
#if PLATFORM_ENABLE_VECTORINTRINSICS

class FMeshBuildDataProvider
{
public:

	/** Initialization constructor. */
	FMeshBuildDataProvider(
		const TkDOPTree<const FMeshBuildDataProvider, uint32>& InkDopTree) :
		kDopTree(InkDopTree)
	{}

	// kDOP data provider interface.

	FORCEINLINE const TkDOPTree<const FMeshBuildDataProvider, uint32>& GetkDOPTree(void) const
	{
		return kDopTree;
	}

	FORCEINLINE const FMatrix& GetLocalToWorld(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE const FMatrix& GetWorldToLocal(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE FMatrix GetLocalToWorldTransposeAdjoint(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE float GetDeterminant(void) const
	{
		return 1.0f;
	}

private:

	const TkDOPTree<const FMeshBuildDataProvider, uint32>& kDopTree;
};

/** Generates unit length, stratified and uniformly distributed direction samples in a hemisphere. */
void GenerateStratifiedUniformHemisphereSamples(int32 NumThetaSteps, int32 NumPhiSteps, FRandomStream& RandomStream, TArray<FVector4>& Samples)
{
	Samples.Empty(NumThetaSteps * NumPhiSteps);
	for (int32 ThetaIndex = 0; ThetaIndex < NumThetaSteps; ThetaIndex++)
	{
		for (int32 PhiIndex = 0; PhiIndex < NumPhiSteps; PhiIndex++)
		{
			const float U1 = RandomStream.GetFraction();
			const float U2 = RandomStream.GetFraction();

			const float Fraction1 = (ThetaIndex + U1) / (float)NumThetaSteps;
			const float Fraction2 = (PhiIndex + U2) / (float)NumPhiSteps;

			const float R = FMath::Sqrt(1.0f - Fraction1 * Fraction1);

			const float Phi = 2.0f * (float)PI * Fraction2;
			// Convert to Cartesian
			Samples.Add(FVector4(FMath::Cos(Phi) * R, FMath::Sin(Phi) * R, Fraction1));
		}
	}
}

class FMeshDistanceFieldAsyncTask : public FNonAbandonableTask
{
public:
	FMeshDistanceFieldAsyncTask(TkDOPTree<const FMeshBuildDataProvider, uint32>* InkDopTree,
		const TArray<FVector4>* InSampleDirections,
		FBox InVolumeBounds,
		FIntVector InVolumeDimensions,
		float InVolumeMaxDistance,
		int32 InZIndex,
		TArray<FFloat16>* DistanceFieldVolume)
		:
		kDopTree(InkDopTree),
		SampleDirections(InSampleDirections),
		VolumeBounds(InVolumeBounds),
		VolumeDimensions(InVolumeDimensions),
		VolumeMaxDistance(InVolumeMaxDistance),
		ZIndex(InZIndex),
		OutDistanceFieldVolume(DistanceFieldVolume),
		bNegativeAtBorder(false)
	{}

	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMeshDistanceFieldAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	bool WasNegativeAtBorder() const
	{
		return bNegativeAtBorder;
	}

private:

	// Readonly inputs
	TkDOPTree<const FMeshBuildDataProvider, uint32>* kDopTree;
	const TArray<FVector4>* SampleDirections;
	FBox VolumeBounds;
	FIntVector VolumeDimensions;
	float VolumeMaxDistance;
	int32 ZIndex;

	// Output
	TArray<FFloat16>* OutDistanceFieldVolume;
	bool bNegativeAtBorder;
};

void FMeshDistanceFieldAsyncTask::DoWork()
{
	FMeshBuildDataProvider kDOPDataProvider(*kDopTree);
	const FVector DistanceFieldVoxelSize(VolumeBounds.GetSize() / FVector(VolumeDimensions.X, VolumeDimensions.Y, VolumeDimensions.Z));
	const float VoxelDiameterSqr = DistanceFieldVoxelSize.SizeSquared();

	for (int32 YIndex = 0; YIndex < VolumeDimensions.Y; YIndex++)
	{
		for (int32 XIndex = 0; XIndex < VolumeDimensions.X; XIndex++)
		{
			const FVector VoxelPosition = FVector(XIndex + .5f, YIndex + .5f, ZIndex + .5f) * DistanceFieldVoxelSize + VolumeBounds.Min;
			const int32 Index = (ZIndex * VolumeDimensions.Y * VolumeDimensions.X + YIndex * VolumeDimensions.X + XIndex);

			float MinDistance = VolumeMaxDistance;
			int32 Hit = 0;
			int32 HitBack = 0;

			for (int32 SampleIndex = 0; SampleIndex < SampleDirections->Num(); SampleIndex++)
			{
				const FVector RayDirection = (*SampleDirections)[SampleIndex];

				if (FMath::LineBoxIntersection(VolumeBounds, VoxelPosition, VoxelPosition + RayDirection * VolumeMaxDistance, RayDirection))
				{
					FkHitResult Result;

					TkDOPLineCollisionCheck<const FMeshBuildDataProvider, uint32> kDOPCheck(
						VoxelPosition,
						VoxelPosition + RayDirection * VolumeMaxDistance,
						true,
						kDOPDataProvider,
						&Result);

					bool bHit = kDopTree->LineCheck(kDOPCheck);

					if (bHit)
					{
						Hit++;

						const FVector HitNormal = kDOPCheck.GetHitNormal();

						if (FVector::DotProduct(RayDirection, HitNormal) > 0
							// MaterialIndex on the build triangles was set to 1 if two-sided, or 0 if one-sided
							&& kDOPCheck.Result->Item == 0)
						{
							HitBack++;
						}

						const float CurrentDistance = VolumeMaxDistance * Result.Time;

						if (CurrentDistance < MinDistance)
						{
							MinDistance = CurrentDistance;
						}
					}
				}
			}

			const float UnsignedDistance = MinDistance;

			// Consider this voxel 'inside' an object if more than 50% of the rays hit back faces
			MinDistance *= (Hit == 0 || HitBack < SampleDirections->Num() * .5f) ? 1 : -1;

			// If we are very close to a surface and nearly all of our rays hit backfaces, treat as inside
			// This is important for one sided planes
			if (FMath::Square(UnsignedDistance) < VoxelDiameterSqr && HitBack > .95f * Hit)
			{
				MinDistance = -UnsignedDistance;
			}

			const float VolumeSpaceDistance = MinDistance / VolumeBounds.GetExtent().GetMax();

			if (MinDistance < 0 &&
				(XIndex == 0 || XIndex == VolumeDimensions.X - 1 ||
				YIndex == 0 || YIndex == VolumeDimensions.Y - 1 ||
				ZIndex == 0 || ZIndex == VolumeDimensions.Z - 1))
			{
				bNegativeAtBorder = true;
			}

			(*OutDistanceFieldVolume)[Index] = FFloat16(VolumeSpaceDistance);
		}
	}
}

void FMeshUtilities::GenerateSignedDistanceFieldVolumeData(
	const FStaticMeshLODResources& LODModel,
class FQueuedThreadPool& ThreadPool,
	const TArray<EBlendMode>& MaterialBlendModes,
	const FBoxSphereBounds& Bounds,
	float DistanceFieldResolutionScale,
	bool bGenerateAsIfTwoSided,
	FDistanceFieldVolumeData& OutData)
{
	if (DistanceFieldResolutionScale > 0)
	{
		const double StartTime = FPlatformTime::Seconds();
		const FPositionVertexBuffer& PositionVertexBuffer = LODModel.PositionVertexBuffer;
		FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
		TArray<FkDOPBuildCollisionTriangle<uint32> > BuildTriangles;

		FVector BoundsSize = Bounds.GetBox().GetExtent() * 2;
		float MaxDimension = FMath::Max(FMath::Max(BoundsSize.X, BoundsSize.Y), BoundsSize.Z);

		// Consider the mesh a plane if it is very flat
		const bool bMeshWasPlane = BoundsSize.Z * 100 < MaxDimension
			// And it lies mostly on the origin
			&& Bounds.Origin.Z - Bounds.BoxExtent.Z < KINDA_SMALL_NUMBER
			&& Bounds.Origin.Z + Bounds.BoxExtent.Z > -KINDA_SMALL_NUMBER;

		for (int32 i = 0; i < Indices.Num(); i += 3)
		{
			FVector V0 = PositionVertexBuffer.VertexPosition(Indices[i + 0]);
			FVector V1 = PositionVertexBuffer.VertexPosition(Indices[i + 1]);
			FVector V2 = PositionVertexBuffer.VertexPosition(Indices[i + 2]);

			if (bMeshWasPlane)
			{
				// Flatten out the mesh into an actual plane, this will allow us to manipulate the component's Z scale at runtime without artifacts
				V0.Z = 0;
				V1.Z = 0;
				V2.Z = 0;
			}

			const FVector LocalNormal = ((V1 - V2) ^ (V0 - V2)).GetSafeNormal();

			// No degenerates
			if (LocalNormal.IsUnit())
			{
				bool bTriangleIsOpaqueOrMasked = false;

				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
				{
					const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];

					if ((uint32)i >= Section.FirstIndex && (uint32)i < Section.FirstIndex + Section.NumTriangles * 3)
					{
						if (MaterialBlendModes.IsValidIndex(Section.MaterialIndex))
						{
							bTriangleIsOpaqueOrMasked = !IsTranslucentBlendMode(MaterialBlendModes[Section.MaterialIndex]);
						}

						break;
					}
				}

				if (bTriangleIsOpaqueOrMasked)
				{
					BuildTriangles.Add(FkDOPBuildCollisionTriangle<uint32>(
						bGenerateAsIfTwoSided,
						V0,
						V1,
						V2));
				}
			}
		}

		TkDOPTree<const FMeshBuildDataProvider, uint32> kDopTree;
		kDopTree.Build(BuildTriangles);

		//@todo - project setting
		const int32 NumVoxelDistanceSamples = 1200;
		TArray<FVector4> SampleDirections;
		const int32 NumThetaSteps = FMath::TruncToInt(FMath::Sqrt(NumVoxelDistanceSamples / (2.0f * (float)PI)));
		const int32 NumPhiSteps = FMath::TruncToInt(NumThetaSteps * (float)PI);
		FRandomStream RandomStream(0);
		GenerateStratifiedUniformHemisphereSamples(NumThetaSteps, NumPhiSteps, RandomStream, SampleDirections);
		TArray<FVector4> OtherHemisphereSamples;
		GenerateStratifiedUniformHemisphereSamples(NumThetaSteps, NumPhiSteps, RandomStream, OtherHemisphereSamples);

		for (int32 i = 0; i < OtherHemisphereSamples.Num(); i++)
		{
			FVector4 Sample = OtherHemisphereSamples[i];
			Sample.Z *= -1;
			SampleDirections.Add(Sample);
		}

		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFields.MaxPerMeshResolution"));
		const int32 PerMeshMax = CVar->GetValueOnAnyThread();

		// Meshes with explicit artist-specified scale can go higher
		const int32 MaxNumVoxelsOneDim = DistanceFieldResolutionScale <= 1 ? PerMeshMax / 2 : PerMeshMax;
		const int32 MinNumVoxelsOneDim = 8;

		static const auto CVarDensity = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.DistanceFields.DefaultVoxelDensity"));
		const float VoxelDensity = CVarDensity->GetValueOnAnyThread();

		const float NumVoxelsPerLocalSpaceUnit = VoxelDensity * DistanceFieldResolutionScale;
		FBox MeshBounds(Bounds.GetBox());

		{
			const float MaxOriginalExtent = MeshBounds.GetExtent().GetMax();
			// Expand so that the edges of the volume are guaranteed to be outside of the mesh
			// Any samples outside the bounds will be clamped to the border, so they must be outside
			const FVector NewExtent(MeshBounds.GetExtent() + FVector(.2f * MaxOriginalExtent).ComponentMax(4 * MeshBounds.GetExtent() / MinNumVoxelsOneDim));
			FBox DistanceFieldVolumeBounds = FBox(MeshBounds.GetCenter() - NewExtent, MeshBounds.GetCenter() + NewExtent);
			const float DistanceFieldVolumeMaxDistance = DistanceFieldVolumeBounds.GetExtent().Size();

			const FVector DesiredDimensions(DistanceFieldVolumeBounds.GetSize() * FVector(NumVoxelsPerLocalSpaceUnit));

			const FIntVector VolumeDimensions(
				FMath::Clamp(FMath::TruncToInt(DesiredDimensions.X), MinNumVoxelsOneDim, MaxNumVoxelsOneDim),
				FMath::Clamp(FMath::TruncToInt(DesiredDimensions.Y), MinNumVoxelsOneDim, MaxNumVoxelsOneDim),
				FMath::Clamp(FMath::TruncToInt(DesiredDimensions.Z), MinNumVoxelsOneDim, MaxNumVoxelsOneDim));

			OutData.Size = VolumeDimensions;
			OutData.LocalBoundingBox = DistanceFieldVolumeBounds;
			OutData.DistanceFieldVolume.AddZeroed(VolumeDimensions.X * VolumeDimensions.Y * VolumeDimensions.Z);

			TIndirectArray<FAsyncTask<FMeshDistanceFieldAsyncTask>> AsyncTasks;

			for (int32 ZIndex = 0; ZIndex < VolumeDimensions.Z; ZIndex++)
			{
				FAsyncTask<FMeshDistanceFieldAsyncTask>* Task = new FAsyncTask<class FMeshDistanceFieldAsyncTask>(
					&kDopTree,
					&SampleDirections,
					DistanceFieldVolumeBounds,
					VolumeDimensions,
					DistanceFieldVolumeMaxDistance,
					ZIndex,
					&OutData.DistanceFieldVolume);

				Task->StartBackgroundTask(&ThreadPool);

				AsyncTasks.Add(Task);
			}

			bool bNegativeAtBorder = false;

			for (int32 TaskIndex = 0; TaskIndex < AsyncTasks.Num(); TaskIndex++)
			{
				FAsyncTask<FMeshDistanceFieldAsyncTask>& Task = AsyncTasks[TaskIndex];
				Task.EnsureCompletion(false);
				bNegativeAtBorder = bNegativeAtBorder || Task.GetTask().WasNegativeAtBorder();
			}

			OutData.bMeshWasClosed = !bNegativeAtBorder;
			OutData.bBuiltAsIfTwoSided = bGenerateAsIfTwoSided;
			OutData.bMeshWasPlane = bMeshWasPlane;

			UE_LOG(LogMeshUtilities, Log, TEXT("Finished distance field build in %.1fs - %ux%ux%u distance field, %u triangles"),
				(float)(FPlatformTime::Seconds() - StartTime),
				VolumeDimensions.X,
				VolumeDimensions.Y,
				VolumeDimensions.Z,
				Indices.Num() / 3);

			// Toss distance field if mesh was not closed
			if (bNegativeAtBorder)
			{
				OutData.Size = FIntVector(0, 0, 0);
				OutData.DistanceFieldVolume.Empty();

				UE_LOG(LogMeshUtilities, Log, TEXT("Discarded distance field as mesh was not closed!  Assign a two-sided material to fix."));
			}
		}
	}
}

#else

void FMeshUtilities::GenerateSignedDistanceFieldVolumeData(
	const FStaticMeshLODResources& LODModel,
class FQueuedThreadPool& ThreadPool,
	const TArray<EBlendMode>& MaterialBlendModes,
	const FBoxSphereBounds& Bounds,
	float DistanceFieldResolutionScale,
	bool bGenerateAsIfTwoSided,
	FDistanceFieldVolumeData& OutData)
{
	if (DistanceFieldResolutionScale > 0)
	{
		UE_LOG(LogMeshUtilities, Error, TEXT("Couldn't generate distance field for mesh, platform is missing required Vector intrinsics."));
	}
}

#endif

/*------------------------------------------------------------------------------
NVTriStrip for cache optimizing index buffers.
------------------------------------------------------------------------------*/

namespace NvTriStrip
{
	/**
	* Converts 16 bit indices to 32 bit prior to passing them into the real GenerateStrips util method
	*/
	void GenerateStrips(
		const uint8* Indices,
		bool Is32Bit,
		const uint32 NumIndices,
		PrimitiveGroup** PrimGroups,
		uint32* NumGroups
		)
	{
		if (Is32Bit)
		{
			GenerateStrips((uint32*)Indices, NumIndices, PrimGroups, NumGroups);
		}
		else
		{
			// convert to 32 bit
			uint32 Idx;
			TArray<uint32> NewIndices;
			NewIndices.AddUninitialized(NumIndices);
			for (Idx = 0; Idx < NumIndices; ++Idx)
			{
				NewIndices[Idx] = ((uint16*)Indices)[Idx];
			}
			GenerateStrips(NewIndices.GetData(), NumIndices, PrimGroups, NumGroups);
		}

	}

	/**
	* Orders a triangle list for better vertex cache coherency.
	*
	* *** WARNING: This is safe to call for multiple threads IF AND ONLY IF all
	* threads call SetListsOnly(true) and SetCacheSize(CACHESIZE_GEFORCE3). If
	* NvTriStrip is ever used with different settings the library will need
	* some modifications to be thread-safe. ***
	*/
	template<typename IndexDataType, typename Allocator>
	void CacheOptimizeIndexBuffer(TArray<IndexDataType, Allocator>& Indices)
	{
		static_assert(sizeof(IndexDataType) == 2 || sizeof(IndexDataType) == 4, "Indices must be short or int.");

		PrimitiveGroup*	PrimitiveGroups = NULL;
		uint32			NumPrimitiveGroups = 0;
		bool Is32Bit = sizeof(IndexDataType) == 4;

		SetListsOnly(true);
		SetCacheSize(CACHESIZE_GEFORCE3);

		GenerateStrips((uint8*)Indices.GetData(), Is32Bit, Indices.Num(), &PrimitiveGroups, &NumPrimitiveGroups);

		Indices.Empty();
		Indices.AddUninitialized(PrimitiveGroups->numIndices);

		if (Is32Bit)
		{
			FMemory::Memcpy(Indices.GetData(), PrimitiveGroups->indices, Indices.Num() * sizeof(IndexDataType));
		}
		else
		{
			for (uint32 I = 0; I < PrimitiveGroups->numIndices; ++I)
			{
				Indices[I] = (uint16)PrimitiveGroups->indices[I];
			}
		}

		delete[] PrimitiveGroups;
	}
}

/*------------------------------------------------------------------------------
Forsyth algorithm for cache optimizing index buffers.
------------------------------------------------------------------------------*/

namespace Forsyth
{
	/**
	* Converts 16 bit indices to 32 bit prior to passing them into the real OptimizeFaces util method
	*/
	void OptimizeFaces(
		const uint8* Indices,
		bool Is32Bit,
		const uint32 NumIndices,
		uint32 NumVertices,
		uint32* OutIndices,
		uint16 CacheSize
		)
	{
		if (Is32Bit)
		{
			OptimizeFaces((uint32*)Indices, NumIndices, NumVertices, OutIndices, CacheSize);
		}
		else
		{
			// convert to 32 bit
			uint32 Idx;
			TArray<uint32> NewIndices;
			NewIndices.AddUninitialized(NumIndices);
			for (Idx = 0; Idx < NumIndices; ++Idx)
			{
				NewIndices[Idx] = ((uint16*)Indices)[Idx];
			}
			OptimizeFaces(NewIndices.GetData(), NumIndices, NumVertices, OutIndices, CacheSize);
		}

	}

	/**
	* Orders a triangle list for better vertex cache coherency.
	*/
	template<typename IndexDataType, typename Allocator>
	void CacheOptimizeIndexBuffer(TArray<IndexDataType, Allocator>& Indices)
	{
		static_assert(sizeof(IndexDataType) == 2 || sizeof(IndexDataType) == 4, "Indices must be short or int.");
		bool Is32Bit = sizeof(IndexDataType) == 4;

		// Count the number of vertices
		uint32 NumVertices = 0;
		for (int32 Index = 0; Index < Indices.Num(); ++Index)
		{
			if (Indices[Index] > NumVertices)
			{
				NumVertices = Indices[Index];
			}
		}
		NumVertices += 1;

		TArray<uint32> OptimizedIndices;
		OptimizedIndices.AddUninitialized(Indices.Num());
		uint16 CacheSize = 32;
		OptimizeFaces((uint8*)Indices.GetData(), Is32Bit, Indices.Num(), NumVertices, OptimizedIndices.GetData(), CacheSize);

		if (Is32Bit)
		{
			FMemory::Memcpy(Indices.GetData(), OptimizedIndices.GetData(), Indices.Num() * sizeof(IndexDataType));
		}
		else
		{
			for (int32 I = 0; I < OptimizedIndices.Num(); ++I)
			{
				Indices[I] = (uint16)OptimizedIndices[I];
			}
		}
	}
}

void FMeshUtilities::CacheOptimizeIndexBuffer(TArray<uint16>& Indices)
{
	if (bUsingNvTriStrip)
	{
		NvTriStrip::CacheOptimizeIndexBuffer(Indices);
	}
	else if (!bDisableTriangleOrderOptimization)
	{
		Forsyth::CacheOptimizeIndexBuffer(Indices);
	}
}

void FMeshUtilities::CacheOptimizeIndexBuffer(TArray<uint32>& Indices)
{
	if (bUsingNvTriStrip)
	{
		NvTriStrip::CacheOptimizeIndexBuffer(Indices);
	}
	else if (!bDisableTriangleOrderOptimization)
	{
		Forsyth::CacheOptimizeIndexBuffer(Indices);
	}
}

/*------------------------------------------------------------------------------
NVTessLib for computing adjacency used for tessellation.
------------------------------------------------------------------------------*/

/**
* Provides static mesh render data to the NVIDIA tessellation library.
*/
class FStaticMeshNvRenderBuffer : public nv::RenderBuffer
{
public:

	/** Construct from static mesh render buffers. */
	FStaticMeshNvRenderBuffer(
		const FPositionVertexBuffer& InPositionVertexBuffer,
		const FStaticMeshVertexBuffer& InVertexBuffer,
		const TArray<uint32>& Indices)
		: PositionVertexBuffer(InPositionVertexBuffer)
		, VertexBuffer(InVertexBuffer)
	{
		check(PositionVertexBuffer.GetNumVertices() == VertexBuffer.GetNumVertices());
		mIb = new nv::IndexBuffer((void*)Indices.GetData(), nv::IBT_U32, Indices.Num(), false);
	}

	/** Retrieve the position and first texture coordinate of the specified index. */
	virtual nv::Vertex getVertex(unsigned int Index) const
	{
		nv::Vertex Vertex;

		check(Index < PositionVertexBuffer.GetNumVertices());

		const FVector& Position = PositionVertexBuffer.VertexPosition(Index);
		Vertex.pos.x = Position.X;
		Vertex.pos.y = Position.Y;
		Vertex.pos.z = Position.Z;

		if (VertexBuffer.GetNumTexCoords())
		{
			const FVector2D UV = VertexBuffer.GetVertexUV(Index, 0);
			Vertex.uv.x = UV.X;
			Vertex.uv.y = UV.Y;
		}
		else
		{
			Vertex.uv.x = 0.0f;
			Vertex.uv.y = 0.0f;
		}

		return Vertex;
	}

private:

	/** The position vertex buffer for the static mesh. */
	const FPositionVertexBuffer& PositionVertexBuffer;

	/** The vertex buffer for the static mesh. */
	const FStaticMeshVertexBuffer& VertexBuffer;

	/** Copying is forbidden. */
	FStaticMeshNvRenderBuffer(const FStaticMeshNvRenderBuffer&);
	FStaticMeshNvRenderBuffer& operator=(const FStaticMeshNvRenderBuffer&);
};

/**
* Provides skeletal mesh render data to the NVIDIA tessellation library.
*/
class FSkeletalMeshNvRenderBuffer : public nv::RenderBuffer
{
public:

	/** Construct from static mesh render buffers. */
	FSkeletalMeshNvRenderBuffer(
		const TArray<FSoftSkinVertex>& InVertexBuffer,
		const uint32 InTexCoordCount,
		const TArray<uint32>& Indices)
		: VertexBuffer(InVertexBuffer)
		, TexCoordCount(InTexCoordCount)
	{
		mIb = new nv::IndexBuffer((void*)Indices.GetData(), nv::IBT_U32, Indices.Num(), false);
	}

	/** Retrieve the position and first texture coordinate of the specified index. */
	virtual nv::Vertex getVertex(unsigned int Index) const
	{
		nv::Vertex Vertex;

		check(Index < (unsigned int)VertexBuffer.Num());

		const FSoftSkinVertex& SrcVertex = VertexBuffer[Index];

		Vertex.pos.x = SrcVertex.Position.X;
		Vertex.pos.y = SrcVertex.Position.Y;
		Vertex.pos.z = SrcVertex.Position.Z;

		if (TexCoordCount > 0)
		{
			Vertex.uv.x = SrcVertex.UVs[0].X;
			Vertex.uv.y = SrcVertex.UVs[0].Y;
		}
		else
		{
			Vertex.uv.x = 0.0f;
			Vertex.uv.y = 0.0f;
		}

		return Vertex;
	}

private:
	/** The vertex buffer for the skeletal mesh. */
	const TArray<FSoftSkinVertex>& VertexBuffer;
	const uint32 TexCoordCount;

	/** Copying is forbidden. */
	FSkeletalMeshNvRenderBuffer(const FSkeletalMeshNvRenderBuffer&);
	FSkeletalMeshNvRenderBuffer& operator=(const FSkeletalMeshNvRenderBuffer&);
};

static void BuildStaticAdjacencyIndexBuffer(
	const FPositionVertexBuffer& PositionVertexBuffer,
	const FStaticMeshVertexBuffer& VertexBuffer,
	const TArray<uint32>& Indices,
	TArray<uint32>& OutPnAenIndices
	)
{
	if (Indices.Num())
	{
		FStaticMeshNvRenderBuffer StaticMeshRenderBuffer(PositionVertexBuffer, VertexBuffer, Indices);
		nv::IndexBuffer* PnAENIndexBuffer = nv::tess::buildTessellationBuffer(&StaticMeshRenderBuffer, nv::DBM_PnAenDominantCorner, true);
		check(PnAENIndexBuffer);
		const int32 IndexCount = (int32)PnAENIndexBuffer->getLength();
		OutPnAenIndices.Empty(IndexCount);
		OutPnAenIndices.AddUninitialized(IndexCount);
		for (int32 Index = 0; Index < IndexCount; ++Index)
		{
			OutPnAenIndices[Index] = (*PnAENIndexBuffer)[Index];
		}
		delete PnAENIndexBuffer;
	}
	else
	{
		OutPnAenIndices.Empty();
	}
}

void FMeshUtilities::BuildSkeletalAdjacencyIndexBuffer(
	const TArray<FSoftSkinVertex>& VertexBuffer,
	const uint32 TexCoordCount,
	const TArray<uint32>& Indices,
	TArray<uint32>& OutPnAenIndices
	)
{
	if (Indices.Num())
	{
		FSkeletalMeshNvRenderBuffer SkeletalMeshRenderBuffer(VertexBuffer, TexCoordCount, Indices);
		nv::IndexBuffer* PnAENIndexBuffer = nv::tess::buildTessellationBuffer(&SkeletalMeshRenderBuffer, nv::DBM_PnAenDominantCorner, true);
		check(PnAENIndexBuffer);
		const int32 IndexCount = (int32)PnAENIndexBuffer->getLength();
		OutPnAenIndices.Empty(IndexCount);
		OutPnAenIndices.AddUninitialized(IndexCount);
		for (int32 Index = 0; Index < IndexCount; ++Index)
		{
			OutPnAenIndices[Index] = (*PnAENIndexBuffer)[Index];
		}
		delete PnAENIndexBuffer;
	}
	else
	{
		OutPnAenIndices.Empty();
	}
}

void FMeshUtilities::RechunkSkeletalMeshModels(USkeletalMesh* SrcMesh, int32 MaxBonesPerChunk)
{
#if WITH_EDITORONLY_DATA
	TIndirectArray<FStaticLODModel> DestModels;
	TIndirectArray<FSkinnedModelData> ModelData;
	FReferenceSkeleton RefSkeleton = SrcMesh->RefSkeleton;
	uint32 VertexBufferBuildFlags = SrcMesh->GetVertexBufferFlags();
	FSkeletalMeshResource* SrcMeshResource = SrcMesh->GetImportedResource();
	FVector TriangleSortCenter;
	bool bHaveTriangleSortCenter = SrcMesh->GetSortCenterPoint(TriangleSortCenter);

	for (int32 ModelIndex = 0; ModelIndex < SrcMeshResource->LODModels.Num(); ++ModelIndex)
	{
		FSkinnedModelData& TmpModelData = *new(ModelData)FSkinnedModelData();
		SkeletalMeshTools::CopySkinnedModelData(TmpModelData, SrcMeshResource->LODModels[ModelIndex]);
	}

	for (int32 ModelIndex = 0; ModelIndex < ModelData.Num(); ++ModelIndex)
	{
		TArray<FSkinnedMeshChunk*> Chunks;
		TArray<int32> PointToOriginalMap;
		TArray<ETriangleSortOption> SectionSortOptions;

		const FSkinnedModelData& SrcModel = ModelData[ModelIndex];
		FStaticLODModel& DestModel = *new(DestModels)FStaticLODModel();

		SkeletalMeshTools::UnchunkSkeletalModel(Chunks, PointToOriginalMap, SrcModel);
		SkeletalMeshTools::ChunkSkinnedVertices(Chunks, MaxBonesPerChunk);

		for (int32 ChunkIndex = 0; ChunkIndex < Chunks.Num(); ++ChunkIndex)
		{
			int32 SectionIndex = Chunks[ChunkIndex]->OriginalSectionIndex;
			SectionSortOptions.Add(SrcModel.Sections[SectionIndex].TriangleSorting);
		}
		check(SectionSortOptions.Num() == Chunks.Num());

		BuildSkeletalModelFromChunks(DestModel, RefSkeleton, Chunks, PointToOriginalMap);
		check(DestModel.Sections.Num() == SectionSortOptions.Num());

		DestModel.NumTexCoords = SrcModel.NumTexCoords;
		DestModel.BuildVertexBuffers(VertexBufferBuildFlags);
		for (int32 SectionIndex = 0; SectionIndex < DestModel.Sections.Num(); ++SectionIndex)
		{
			DestModel.SortTriangles(TriangleSortCenter, bHaveTriangleSortCenter, SectionIndex, SectionSortOptions[SectionIndex]);
		}
	}

	//@todo-rco: Swap() doesn't seem to work
	Exchange(SrcMeshResource->LODModels, DestModels);

	// TODO: Also need to patch bEnableShadowCasting in the LODInfo struct.
#endif // #if WITH_EDITORONLY_DATA
}

void FMeshUtilities::CalcBoneVertInfos(USkeletalMesh* SkeletalMesh, TArray<FBoneVertInfo>& Infos, bool bOnlyDominant)
{
	SkeletalMeshTools::CalcBoneVertInfos(SkeletalMesh, Infos, bOnlyDominant);
}

/**
* Builds a renderable skeletal mesh LOD model. Note that the array of chunks
* will be destroyed during this process!
* @param LODModel				Upon return contains a renderable skeletal mesh LOD model.
* @param RefSkeleton			The reference skeleton associated with the model.
* @param Chunks				Skinned mesh chunks from which to build the renderable model.
* @param PointToOriginalMap	Maps a vertex's RawPointIdx to its index at import time.
*/
void FMeshUtilities::BuildSkeletalModelFromChunks(FStaticLODModel& LODModel, const FReferenceSkeleton& RefSkeleton, TArray<FSkinnedMeshChunk*>& Chunks, const TArray<int32>& PointToOriginalMap)
{
#if WITH_EDITORONLY_DATA
	// Clear out any data currently held in the LOD model.
	LODModel.Sections.Empty();
	LODModel.NumVertices = 0;
	if (LODModel.MultiSizeIndexContainer.IsIndexBufferValid())
	{
		LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Empty();
	}

	// Setup the section and chunk arrays on the model.
	for (int32 ChunkIndex = 0; ChunkIndex < Chunks.Num(); ++ChunkIndex)
	{
		FSkinnedMeshChunk* SrcChunk = Chunks[ChunkIndex];

		FSkelMeshSection& Section = *new(LODModel.Sections) FSkelMeshSection();
		Section.MaterialIndex = SrcChunk->MaterialIndex;
		Exchange(Section.BoneMap, SrcChunk->BoneMap);

		// Update the active bone indices on the LOD model.
		for (int32 BoneIndex = 0; BoneIndex < Section.BoneMap.Num(); ++BoneIndex)
		{
			LODModel.ActiveBoneIndices.AddUnique(Section.BoneMap[BoneIndex]);
		}
	}

	LODModel.ActiveBoneIndices.Sort();

	// Reset 'final vertex to import vertex' map info
	LODModel.MeshToImportVertexMap.Empty();
	LODModel.MaxImportVertex = 0;

	// Keep track of index mapping to chunk vertex offsets
	TArray< TArray<uint32> > VertexIndexRemap;
	VertexIndexRemap.Empty(LODModel.Sections.Num());
	// Pack the chunk vertices into a single vertex buffer.
	TArray<uint32> RawPointIndices;
	LODModel.NumVertices = 0;

	int32 PrevMaterialIndex = -1;
	int32 CurrentChunkBaseVertexIndex = -1; 	// base vertex index for all chunks of the same material
	int32 CurrentChunkVertexCount = -1; 		// total vertex count for all chunks of the same material
	int32 CurrentVertexIndex = 0; 			// current vertex index added to the index buffer for all chunks of the same material

	// rearrange the vert order to minimize the data fetched by the GPU
	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
	{
		if (IsInGameThread())
		{
			GWarn->StatusUpdate(SectionIndex, LODModel.Sections.Num(), NSLOCTEXT("UnrealEd", "ProcessingSections", "Processing Sections"));
		}

		FSkinnedMeshChunk* SrcChunk = Chunks[SectionIndex];
		FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
		TArray<FSoftSkinBuildVertex>& ChunkVertices = SrcChunk->Vertices;
		TArray<uint32>& ChunkIndices = SrcChunk->Indices;

		// Reorder the section index buffer for better vertex cache efficiency.
		CacheOptimizeIndexBuffer(ChunkIndices);

		// Calculate the number of triangles in the section.  Note that CacheOptimize may change the number of triangles in the index buffer!
		Section.NumTriangles = ChunkIndices.Num() / 3;
		TArray<FSoftSkinBuildVertex> OriginalVertices;
		Exchange(ChunkVertices, OriginalVertices);
		ChunkVertices.AddUninitialized(OriginalVertices.Num());

		TArray<int32> IndexCache;
		IndexCache.AddUninitialized(ChunkVertices.Num());
		FMemory::Memset(IndexCache.GetData(), INDEX_NONE, IndexCache.Num() * IndexCache.GetTypeSize());
		int32 NextAvailableIndex = 0;
		// Go through the indices and assign them new values that are coherent where possible
		for (int32 Index = 0; Index < ChunkIndices.Num(); Index++)
		{
			const int32 OriginalIndex = ChunkIndices[Index];
			const int32 CachedIndex = IndexCache[OriginalIndex];

			if (CachedIndex == INDEX_NONE)
			{
				// No new index has been allocated for this existing index, assign a new one
				ChunkIndices[Index] = NextAvailableIndex;
				// Mark what this index has been assigned to
				IndexCache[OriginalIndex] = NextAvailableIndex;
				NextAvailableIndex++;
			}
			else
			{
				// Reuse an existing index assignment
				ChunkIndices[Index] = CachedIndex;
			}
			// Reorder the vertices based on the new index assignment
			ChunkVertices[ChunkIndices[Index]] = OriginalVertices[OriginalIndex];
		}
	}

	// Build the arrays of rigid and soft vertices on the model's chunks.
	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
	{
		FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
		TArray<FSoftSkinBuildVertex>& ChunkVertices = Chunks[SectionIndex]->Vertices;

		if (IsInGameThread())
		{
			// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
			GWarn->StatusUpdate(SectionIndex, LODModel.Sections.Num(), NSLOCTEXT("UnrealEd", "ProcessingChunks", "Processing Chunks"));
		}

		CurrentVertexIndex = 0;
		CurrentChunkVertexCount = 0;
		PrevMaterialIndex = Section.MaterialIndex;

		// Calculate the offset to this chunk's vertices in the vertex buffer.
		Section.BaseVertexIndex = CurrentChunkBaseVertexIndex = LODModel.NumVertices;

		// Update the size of the vertex buffer.
		LODModel.NumVertices += ChunkVertices.Num();

		// Separate the section's vertices into rigid and soft vertices.
		TArray<uint32>& ChunkVertexIndexRemap = *new(VertexIndexRemap)TArray<uint32>();
		ChunkVertexIndexRemap.AddUninitialized(ChunkVertices.Num());

		for (int32 VertexIndex = 0; VertexIndex < ChunkVertices.Num(); VertexIndex++)
		{
			const FSoftSkinBuildVertex& SoftVertex = ChunkVertices[VertexIndex];

			FSoftSkinVertex NewVertex;
			NewVertex.Position = SoftVertex.Position;
			NewVertex.TangentX = SoftVertex.TangentX;
			NewVertex.TangentY = SoftVertex.TangentY;
			NewVertex.TangentZ = SoftVertex.TangentZ;
			FMemory::Memcpy(NewVertex.UVs, SoftVertex.UVs, sizeof(FVector2D)*MAX_TEXCOORDS);
			NewVertex.Color = SoftVertex.Color;
			for (int32 i = 0; i < MAX_TOTAL_INFLUENCES; ++i)
			{
				// it only adds to the bone map if it has weight on it
				// BoneMap contains only the bones that has influence with weight of >0.f
				// so here, just make sure it is included before setting the data
				if (Section.BoneMap.IsValidIndex(SoftVertex.InfluenceBones[i]))
				{
					NewVertex.InfluenceBones[i] = SoftVertex.InfluenceBones[i];
					NewVertex.InfluenceWeights[i] = SoftVertex.InfluenceWeights[i];
				}
			}
			Section.SoftVertices.Add(NewVertex);
			ChunkVertexIndexRemap[VertexIndex] = (uint32)(Section.BaseVertexIndex + CurrentVertexIndex);
			CurrentVertexIndex++;
			// add the index to the original wedge point source of this vertex
			RawPointIndices.Add(SoftVertex.PointWedgeIdx);
			// Also remember import index
			const int32 RawVertIndex = PointToOriginalMap[SoftVertex.PointWedgeIdx];
			LODModel.MeshToImportVertexMap.Add(RawVertIndex);
			LODModel.MaxImportVertex = FMath::Max<float>(LODModel.MaxImportVertex, RawVertIndex);
		}

		// update NumVertices
		Section.NumVertices = Section.SoftVertices.Num();

		// update max bone influences
		Section.CalcMaxBoneInfluences();

		// Log info about the chunk.
		UE_LOG(LogSkeletalMesh, Log, TEXT("Section %u: %u vertices, %u active bones"),
			SectionIndex,
			Section.GetNumVertices(),
			Section.BoneMap.Num()
			);
	}

	// Copy raw point indices to LOD model.
	LODModel.RawPointIndices.RemoveBulkData();
	if (RawPointIndices.Num())
	{
		LODModel.RawPointIndices.Lock(LOCK_READ_WRITE);
		void* Dest = LODModel.RawPointIndices.Realloc(RawPointIndices.Num());
		FMemory::Memcpy(Dest, RawPointIndices.GetData(), LODModel.RawPointIndices.GetBulkDataSize());
		LODModel.RawPointIndices.Unlock();
	}

#if DISALLOW_32BIT_INDICES
	LODModel.MultiSizeIndexContainer.CreateIndexBuffer(sizeof(uint16));
#else
	LODModel.MultiSizeIndexContainer.CreateIndexBuffer((LODModel.NumVertices < MAX_uint16) ? sizeof(uint16) : sizeof(uint32));
#endif

	// Finish building the sections.
	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
	{
		FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

		const TArray<uint32>& SectionIndices = Chunks[SectionIndex]->Indices;
		FRawStaticIndexBuffer16or32Interface* IndexBuffer = LODModel.MultiSizeIndexContainer.GetIndexBuffer();
		Section.BaseIndex = IndexBuffer->Num();
		const int32 NumIndices = SectionIndices.Num();
		const TArray<uint32>& SectionVertexIndexRemap = VertexIndexRemap[SectionIndex];
		for (int32 Index = 0; Index < NumIndices; Index++)
		{
			uint32 VertexIndex = SectionVertexIndexRemap[SectionIndices[Index]];
			IndexBuffer->AddItem(VertexIndex);
		}
	}

	// Free the skinned mesh chunks which are no longer needed.
	for (int32 i = 0; i < Chunks.Num(); ++i)
	{
		delete Chunks[i];
		Chunks[i] = NULL;
	}
	Chunks.Empty();

	// Build the adjacency index buffer used for tessellation.
	{
		TArray<FSoftSkinVertex> Vertices;
		LODModel.GetVertices(Vertices);

		FMultiSizeIndexContainerData IndexData;
		LODModel.MultiSizeIndexContainer.GetIndexBufferData(IndexData);

		FMultiSizeIndexContainerData AdjacencyIndexData;
		AdjacencyIndexData.DataTypeSize = IndexData.DataTypeSize;

		BuildSkeletalAdjacencyIndexBuffer(Vertices, LODModel.NumTexCoords, IndexData.Indices, AdjacencyIndexData.Indices);
		LODModel.AdjacencyMultiSizeIndexContainer.RebuildIndexBuffer(AdjacencyIndexData);
	}

	// Compute the required bones for this model.
	USkeletalMesh::CalculateRequiredBones(LODModel, RefSkeleton, NULL);
#endif // #if WITH_EDITORONLY_DATA
}

/*------------------------------------------------------------------------------
Common functionality.
------------------------------------------------------------------------------*/

/** Helper struct for building acceleration structures. */
struct FIndexAndZ
{
	float Z;
	int32 Index;

	/** Default constructor. */
	FIndexAndZ() {}

	/** Initialization constructor. */
	FIndexAndZ(int32 InIndex, FVector V)
	{
		Z = 0.30f * V.X + 0.33f * V.Y + 0.37f * V.Z;
		Index = InIndex;
	}
};

/** Sorting function for vertex Z/index pairs. */
struct FCompareIndexAndZ
{
	FORCEINLINE bool operator()(FIndexAndZ const& A, FIndexAndZ const& B) const { return A.Z < B.Z; }
};

static int32 ComputeNumTexCoords(FRawMesh const& RawMesh, int32 MaxSupportedTexCoords)
{
	int32 NumWedges = RawMesh.WedgeIndices.Num();
	int32 NumTexCoords = 0;
	for (int32 TexCoordIndex = 0; TexCoordIndex < MAX_MESH_TEXTURE_COORDS; ++TexCoordIndex)
	{
		if (RawMesh.WedgeTexCoords[TexCoordIndex].Num() != NumWedges)
		{
			break;
		}
		NumTexCoords++;
	}
	return FMath::Min(NumTexCoords, MaxSupportedTexCoords);
}

/**
* Returns true if the specified points are about equal
*/
inline bool PointsEqual(const FVector& V1, const FVector& V2, float ComparisonThreshold)
{
	if (FMath::Abs(V1.X - V2.X) > ComparisonThreshold
		|| FMath::Abs(V1.Y - V2.Y) > ComparisonThreshold
		|| FMath::Abs(V1.Z - V2.Z) > ComparisonThreshold)
	{
		return false;
	}
	return true;
}

inline bool UVsEqual(const FVector2D& UV1, const FVector2D& UV2)
{
	if (FMath::Abs(UV1.X - UV2.X) > (1.0f / 1024.0f))
		return false;

	if (FMath::Abs(UV1.Y - UV2.Y) > (1.0f / 1024.0f))
		return false;

	return true;
}

static inline FVector GetPositionForWedge(FRawMesh const& Mesh, int32 WedgeIndex)
{
	int32 VertexIndex = Mesh.WedgeIndices[WedgeIndex];
	return Mesh.VertexPositions[VertexIndex];
}

struct FMeshEdge
{
	int32	Vertices[2];
	int32	Faces[2];
};

/**
* This helper class builds the edge list for a mesh. It uses a hash of vertex
* positions to edges sharing that vertex to remove the n^2 searching of all
* previously added edges. This class is templatized so it can be used with
* either static mesh or skeletal mesh vertices
*/
template <class VertexClass> class TEdgeBuilder
{
protected:
	/**
	* The list of indices to build the edge data from
	*/
	const TArray<uint32>& Indices;
	/**
	* The array of verts for vertex position comparison
	*/
	const TArray<VertexClass>& Vertices;
	/**
	* The array of edges to create
	*/
	TArray<FMeshEdge>& Edges;
	/**
	* List of edges that start with a given vertex
	*/
	TMultiMap<FVector, FMeshEdge*> VertexToEdgeList;

	/**
	* This function determines whether a given edge matches or not. It must be
	* provided by derived classes since they have the specific information that
	* this class doesn't know about (vertex info, influences, etc)
	*
	* @param Index1 The first index of the edge being checked
	* @param Index2 The second index of the edge
	* @param OtherEdge The edge to compare. Was found via the map
	*
	* @return true if the edge is a match, false otherwise
	*/
	virtual bool DoesEdgeMatch(int32 Index1, int32 Index2, FMeshEdge* OtherEdge) = 0;

	/**
	* Searches the list of edges to see if this one matches an existing and
	* returns a pointer to it if it does
	*
	* @param Index1 the first index to check for
	* @param Index2 the second index to check for
	*
	* @return NULL if no edge was found, otherwise the edge that was found
	*/
	inline FMeshEdge* FindOppositeEdge(int32 Index1, int32 Index2)
	{
		FMeshEdge* Edge = NULL;
		TArray<FMeshEdge*> EdgeList;
		// Search the hash for a corresponding vertex
		VertexToEdgeList.MultiFind(Vertices[Index2].Position, EdgeList);
		// Now search through the array for a match or not
		for (int32 EdgeIndex = 0; EdgeIndex < EdgeList.Num() && Edge == NULL;
			EdgeIndex++)
		{
			FMeshEdge* OtherEdge = EdgeList[EdgeIndex];
			// See if this edge matches the passed in edge
			if (OtherEdge != NULL && DoesEdgeMatch(Index1, Index2, OtherEdge))
			{
				// We have a match
				Edge = OtherEdge;
			}
		}
		return Edge;
	}

	/**
	* Updates an existing edge if found or adds the new edge to the list
	*
	* @param Index1 the first index in the edge
	* @param Index2 the second index in the edge
	* @param Triangle the triangle that this edge was found in
	*/
	inline void AddEdge(int32 Index1, int32 Index2, int32 Triangle)
	{
		// If this edge matches another then just fill the other triangle
		// otherwise add it
		FMeshEdge* OtherEdge = FindOppositeEdge(Index1, Index2);
		if (OtherEdge == NULL)
		{
			// Add a new edge to the array
			int32 EdgeIndex = Edges.AddZeroed();
			Edges[EdgeIndex].Vertices[0] = Index1;
			Edges[EdgeIndex].Vertices[1] = Index2;
			Edges[EdgeIndex].Faces[0] = Triangle;
			Edges[EdgeIndex].Faces[1] = -1;
			// Also add this edge to the hash for faster searches
			// NOTE: This relies on the array never being realloced!
			VertexToEdgeList.Add(Vertices[Index1].Position, &Edges[EdgeIndex]);
		}
		else
		{
			OtherEdge->Faces[1] = Triangle;
		}
	}

public:
	/**
	* Initializes the values for the code that will build the mesh edge list
	*/
	TEdgeBuilder(const TArray<uint32>& InIndices,
		const TArray<VertexClass>& InVertices,
		TArray<FMeshEdge>& OutEdges) :
		Indices(InIndices), Vertices(InVertices), Edges(OutEdges)
	{
		// Presize the array so that there are no extra copies being done
		// when adding edges to it
		Edges.Empty(Indices.Num());
	}

	/**
	* Virtual dtor
	*/
	virtual ~TEdgeBuilder(){}


	/**
	* Uses a hash of indices to edge lists so that it can avoid the n^2 search
	* through the full edge list
	*/
	void FindEdges(void)
	{
		// @todo Handle something other than trilists when building edges
		int32 TriangleCount = Indices.Num() / 3;
		int32 EdgeCount = 0;
		// Work through all triangles building the edges
		for (int32 Triangle = 0; Triangle < TriangleCount; Triangle++)
		{
			// Determine the starting index
			int32 TriangleIndex = Triangle * 3;
			// Get the indices for the triangle
			int32 Index1 = Indices[TriangleIndex];
			int32 Index2 = Indices[TriangleIndex + 1];
			int32 Index3 = Indices[TriangleIndex + 2];
			// Add the first to second edge
			AddEdge(Index1, Index2, Triangle);
			// Now add the second to third
			AddEdge(Index2, Index3, Triangle);
			// Add the third to first edge
			AddEdge(Index3, Index1, Triangle);
		}
	}
};

/**
* This is the static mesh specific version for finding edges
*/
class FStaticMeshEdgeBuilder : public TEdgeBuilder<FStaticMeshBuildVertex>
{
public:
	/**
	* Constructor that passes all work to the parent class
	*/
	FStaticMeshEdgeBuilder(const TArray<uint32>& InIndices,
		const TArray<FStaticMeshBuildVertex>& InVertices,
		TArray<FMeshEdge>& OutEdges) :
		TEdgeBuilder<FStaticMeshBuildVertex>(InIndices, InVertices, OutEdges)
	{
	}

	/**
	* This function determines whether a given edge matches or not for a static mesh
	*
	* @param Index1 The first index of the edge being checked
	* @param Index2 The second index of the edge
	* @param OtherEdge The edge to compare. Was found via the map
	*
	* @return true if the edge is a match, false otherwise
	*/
	bool DoesEdgeMatch(int32 Index1, int32 Index2, FMeshEdge* OtherEdge)
	{
		return Vertices[OtherEdge->Vertices[1]].Position == Vertices[Index1].Position &&
			OtherEdge->Faces[1] == -1;
	}
};

static void ComputeTriangleTangents(
	const TArray<FVector>& InVertices,
	const TArray<uint32>& InIndices,
	const TArray<FVector2D>& InUVs,
	TArray<FVector>& OutTangentX,
	TArray<FVector>& OutTangentY,
	TArray<FVector>& OutTangentZ,
	float ComparisonThreshold
	)
{
	const int32 NumTriangles = InIndices.Num() / 3;
	OutTangentX.Empty(NumTriangles);
	OutTangentY.Empty(NumTriangles);
	OutTangentZ.Empty(NumTriangles);

	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++)
	{
		int32 UVIndex = 0;

		FVector P[3];
		for (int32 i = 0; i < 3; ++i)
		{
			P[i] = InVertices[InIndices[TriangleIndex * 3 + i]];
		}

		const FVector Normal = ((P[1] - P[2]) ^ (P[0] - P[2])).GetSafeNormal(ComparisonThreshold);
		FMatrix	ParameterToLocal(
			FPlane(P[1].X - P[0].X, P[1].Y - P[0].Y, P[1].Z - P[0].Z, 0),
			FPlane(P[2].X - P[0].X, P[2].Y - P[0].Y, P[2].Z - P[0].Z, 0),
			FPlane(P[0].X, P[0].Y, P[0].Z, 0),
			FPlane(0, 0, 0, 1)
			);

		const FVector2D T1 = InUVs[TriangleIndex * 3 + 0];
		const FVector2D T2 = InUVs[TriangleIndex * 3 + 1];
		const FVector2D T3 = InUVs[TriangleIndex * 3 + 2];

		FMatrix ParameterToTexture(
			FPlane(T2.X - T1.X, T2.Y - T1.Y, 0, 0),
			FPlane(T3.X - T1.X, T3.Y - T1.Y, 0, 0),
			FPlane(T1.X, T1.Y, 1, 0),
			FPlane(0, 0, 0, 1)
			);

		// Use InverseSlow to catch singular matrices.  Inverse can miss this sometimes.
		const FMatrix TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;

		OutTangentX.Add(TextureToLocal.TransformVector(FVector(1, 0, 0)).GetSafeNormal());
		OutTangentY.Add(TextureToLocal.TransformVector(FVector(0, 1, 0)).GetSafeNormal());
		OutTangentZ.Add(Normal);

		FVector::CreateOrthonormalBasis(
			OutTangentX[TriangleIndex],
			OutTangentY[TriangleIndex],
			OutTangentZ[TriangleIndex]
			);
	}

	check(OutTangentX.Num() == NumTriangles);
	check(OutTangentY.Num() == NumTriangles);
	check(OutTangentZ.Num() == NumTriangles);
}

static void ComputeTriangleTangents(
	TArray<FVector>& OutTangentX,
	TArray<FVector>& OutTangentY,
	TArray<FVector>& OutTangentZ,
	FRawMesh const& RawMesh,
	float ComparisonThreshold
	)
{
	ComputeTriangleTangents(RawMesh.VertexPositions, RawMesh.WedgeIndices, RawMesh.WedgeTexCoords[0], OutTangentX, OutTangentY, OutTangentZ, ComparisonThreshold);

	/*int32 NumTriangles = RawMesh.WedgeIndices.Num() / 3;
	TriangleTangentX.Empty(NumTriangles);
	TriangleTangentY.Empty(NumTriangles);
	TriangleTangentZ.Empty(NumTriangles);

	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++)
	{
	int32 UVIndex = 0;

	FVector P[3];
	for (int32 i = 0; i < 3; ++i)
	{
	P[i] = GetPositionForWedge(RawMesh, TriangleIndex * 3 + i);
	}

	const FVector Normal = ((P[1] - P[2]) ^ (P[0] - P[2])).GetSafeNormal(ComparisonThreshold);
	FMatrix	ParameterToLocal(
	FPlane(P[1].X - P[0].X, P[1].Y - P[0].Y, P[1].Z - P[0].Z, 0),
	FPlane(P[2].X - P[0].X, P[2].Y - P[0].Y, P[2].Z - P[0].Z, 0),
	FPlane(P[0].X, P[0].Y, P[0].Z, 0),
	FPlane(0, 0, 0, 1)
	);

	FVector2D T1 = RawMesh.WedgeTexCoords[UVIndex][TriangleIndex * 3 + 0];
	FVector2D T2 = RawMesh.WedgeTexCoords[UVIndex][TriangleIndex * 3 + 1];
	FVector2D T3 = RawMesh.WedgeTexCoords[UVIndex][TriangleIndex * 3 + 2];
	FMatrix ParameterToTexture(
	FPlane(T2.X - T1.X, T2.Y - T1.Y, 0, 0),
	FPlane(T3.X - T1.X, T3.Y - T1.Y, 0, 0),
	FPlane(T1.X, T1.Y, 1, 0),
	FPlane(0, 0, 0, 1)
	);

	// Use InverseSlow to catch singular matrices.  Inverse can miss this sometimes.
	const FMatrix TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;

	TriangleTangentX.Add(TextureToLocal.TransformVector(FVector(1, 0, 0)).GetSafeNormal());
	TriangleTangentY.Add(TextureToLocal.TransformVector(FVector(0, 1, 0)).GetSafeNormal());
	TriangleTangentZ.Add(Normal);

	FVector::CreateOrthonormalBasis(
	TriangleTangentX[TriangleIndex],
	TriangleTangentY[TriangleIndex],
	TriangleTangentZ[TriangleIndex]
	);
	}

	check(TriangleTangentX.Num() == NumTriangles);
	check(TriangleTangentY.Num() == NumTriangles);
	check(TriangleTangentZ.Num() == NumTriangles);*/
}

/**
* Create a table that maps the corner of each face to its overlapping corners.
* @param OutOverlappingCorners - Maps a corner index to the indices of all overlapping corners.
* @param RawMesh - The mesh for which to compute overlapping corners.
*/
static void FindOverlappingCorners(
	TMultiMap<int32, int32>& OutOverlappingCorners,
	const TArray<FVector>& InVertices,
	const TArray<uint32>& InIndices,
	float ComparisonThreshold
	)
{
	const int32 NumWedges = InIndices.Num();

	// Create a list of vertex Z/index pairs
	TArray<FIndexAndZ> VertIndexAndZ;
	VertIndexAndZ.Reserve(NumWedges);
	for (int32 WedgeIndex = 0; WedgeIndex < NumWedges; WedgeIndex++)
	{
		new(VertIndexAndZ)FIndexAndZ(WedgeIndex, InVertices[InIndices[WedgeIndex]]);
	}

	// Sort the vertices by z value
	VertIndexAndZ.Sort(FCompareIndexAndZ());

	// Search for duplicates, quickly!
	for (int32 i = 0; i < VertIndexAndZ.Num(); i++)
	{
		// only need to search forward, since we add pairs both ways
		for (int32 j = i + 1; j < VertIndexAndZ.Num(); j++)
		{
			if (FMath::Abs(VertIndexAndZ[j].Z - VertIndexAndZ[i].Z) > ComparisonThreshold)
				break; // can't be any more dups

			const FVector& PositionA = InVertices[InIndices[VertIndexAndZ[i].Index]];
			const FVector& PositionB = InVertices[InIndices[VertIndexAndZ[j].Index]];

			if (PointsEqual(PositionA, PositionB, ComparisonThreshold))
			{
				OutOverlappingCorners.Add(VertIndexAndZ[i].Index, VertIndexAndZ[j].Index);
				OutOverlappingCorners.Add(VertIndexAndZ[j].Index, VertIndexAndZ[i].Index);
			}
		}
	}
}

/**
* Create a table that maps the corner of each face to its overlapping corners.
* @param OutOverlappingCorners - Maps a corner index to the indices of all overlapping corners.
* @param RawMesh - The mesh for which to compute overlapping corners.
*/
static void FindOverlappingCorners(
	TMultiMap<int32, int32>& OutOverlappingCorners,
	FRawMesh const& RawMesh,
	float ComparisonThreshold
	)
{
	FindOverlappingCorners(OutOverlappingCorners, RawMesh.VertexPositions, RawMesh.WedgeIndices, ComparisonThreshold);
}

/**
* Smoothing group interpretation helper structure.
*/
struct FFanFace
{
	int32 FaceIndex;
	int32 LinkedVertexIndex;
	bool bFilled;
	bool bBlendTangents;
	bool bBlendNormals;
};

static void ComputeTangents(
	const TArray<FVector>& InVertices,
	const TArray<uint32>& InIndices,
	const TArray<FVector2D>& InUVs,
	const TArray<uint32>& SmoothingGroupIndices,
	TMultiMap<int32, int32> const& OverlappingCorners,
	TArray<FVector>& OutTangentX,
	TArray<FVector>& OutTangentY,
	TArray<FVector>& OutTangentZ,
	const uint32 TangentOptions
	)
{
	bool bBlendOverlappingNormals = (TangentOptions & ETangentOptions::BlendOverlappingNormals) != 0;
	bool bIgnoreDegenerateTriangles = (TangentOptions & ETangentOptions::IgnoreDegenerateTriangles) != 0;
	float ComparisonThreshold = bIgnoreDegenerateTriangles ? THRESH_POINTS_ARE_SAME : 0.0f;

	// Compute per-triangle tangents.
	TArray<FVector> TriangleTangentX;
	TArray<FVector> TriangleTangentY;
	TArray<FVector> TriangleTangentZ;

	ComputeTriangleTangents(
		InVertices,
		InIndices,
		InUVs,
		TriangleTangentX,
		TriangleTangentY,
		TriangleTangentZ,
		bIgnoreDegenerateTriangles ? SMALL_NUMBER : 0.0f
		);

	// Declare these out here to avoid reallocations.
	TArray<FFanFace> RelevantFacesForCorner[3];
	TArray<int32> AdjacentFaces;
	TArray<int32> DupVerts;

	int32 NumWedges = InIndices.Num();
	int32 NumFaces = NumWedges / 3;

	// Allocate storage for tangents if none were provided.
	if (OutTangentX.Num() != NumWedges)
	{
		OutTangentX.Empty(NumWedges);
		OutTangentX.AddZeroed(NumWedges);
	}
	if (OutTangentY.Num() != NumWedges)
	{
		OutTangentY.Empty(NumWedges);
		OutTangentY.AddZeroed(NumWedges);
	}
	if (OutTangentZ.Num() != NumWedges)
	{
		OutTangentZ.Empty(NumWedges);
		OutTangentZ.AddZeroed(NumWedges);
	}

	for (int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
	{
		int32 WedgeOffset = FaceIndex * 3;
		FVector CornerPositions[3];
		FVector CornerTangentX[3];
		FVector CornerTangentY[3];
		FVector CornerTangentZ[3];

		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			CornerTangentX[CornerIndex] = FVector::ZeroVector;
			CornerTangentY[CornerIndex] = FVector::ZeroVector;
			CornerTangentZ[CornerIndex] = FVector::ZeroVector;
			CornerPositions[CornerIndex] = InVertices[InIndices[WedgeOffset + CornerIndex]];
			RelevantFacesForCorner[CornerIndex].Reset();
		}

		// Don't process degenerate triangles.
		if (PointsEqual(CornerPositions[0], CornerPositions[1], ComparisonThreshold)
			|| PointsEqual(CornerPositions[0], CornerPositions[2], ComparisonThreshold)
			|| PointsEqual(CornerPositions[1], CornerPositions[2], ComparisonThreshold))
		{
			continue;
		}

		// No need to process triangles if tangents already exist.
		bool bCornerHasTangents[3] = { 0 };
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			bCornerHasTangents[CornerIndex] = !OutTangentX[WedgeOffset + CornerIndex].IsZero()
				&& !OutTangentY[WedgeOffset + CornerIndex].IsZero()
				&& !OutTangentZ[WedgeOffset + CornerIndex].IsZero();
		}
		if (bCornerHasTangents[0] && bCornerHasTangents[1] && bCornerHasTangents[2])
		{
			continue;
		}

		// Calculate smooth vertex normals.
		float Determinant = FVector::Triple(
			TriangleTangentX[FaceIndex],
			TriangleTangentY[FaceIndex],
			TriangleTangentZ[FaceIndex]
			);

		// Start building a list of faces adjacent to this face.
		AdjacentFaces.Reset();
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			int32 ThisCornerIndex = WedgeOffset + CornerIndex;
			DupVerts.Reset();
			OverlappingCorners.MultiFind(ThisCornerIndex, DupVerts);
			DupVerts.Add(ThisCornerIndex); // I am a "dup" of myself
			for (int32 k = 0; k < DupVerts.Num(); k++)
			{
				AdjacentFaces.AddUnique(DupVerts[k] / 3);
			}
		}

		// We need to sort these here because the criteria for point equality is
		// exact, so we must ensure the exact same order for all dups.
		AdjacentFaces.Sort();

		// Process adjacent faces
		for (int32 AdjacentFaceIndex = 0; AdjacentFaceIndex < AdjacentFaces.Num(); AdjacentFaceIndex++)
		{
			int32 OtherFaceIndex = AdjacentFaces[AdjacentFaceIndex];
			for (int32 OurCornerIndex = 0; OurCornerIndex < 3; OurCornerIndex++)
			{
				if (bCornerHasTangents[OurCornerIndex])
					continue;

				FFanFace NewFanFace;
				int32 CommonIndexCount = 0;

				// Check for vertices in common.
				if (FaceIndex == OtherFaceIndex)
				{
					CommonIndexCount = 3;
					NewFanFace.LinkedVertexIndex = OurCornerIndex;
				}
				else
				{
					// Check matching vertices against main vertex .
					for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
					{
						if (PointsEqual(
							CornerPositions[OurCornerIndex],
							InVertices[InIndices[OtherFaceIndex * 3 + OtherCornerIndex]],
							ComparisonThreshold
							))
						{
							CommonIndexCount++;
							NewFanFace.LinkedVertexIndex = OtherCornerIndex;
						}
					}
				}

				// Add if connected by at least one point. Smoothing matches are considered later.
				if (CommonIndexCount > 0)
				{
					NewFanFace.FaceIndex = OtherFaceIndex;
					NewFanFace.bFilled = (OtherFaceIndex == FaceIndex); // Starter face for smoothing floodfill.
					NewFanFace.bBlendTangents = NewFanFace.bFilled;
					NewFanFace.bBlendNormals = NewFanFace.bFilled;
					RelevantFacesForCorner[OurCornerIndex].Add(NewFanFace);
				}
			}
		}

		// Find true relevance of faces for a vertex normal by traversing
		// smoothing-group-compatible connected triangle fans around common vertices.
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			if (bCornerHasTangents[CornerIndex])
				continue;

			int32 NewConnections;
			do
			{
				NewConnections = 0;
				for (int32 OtherFaceIdx = 0; OtherFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); OtherFaceIdx++)
				{
					FFanFace& OtherFace = RelevantFacesForCorner[CornerIndex][OtherFaceIdx];
					// The vertex' own face is initially the only face with bFilled == true.
					if (OtherFace.bFilled)
					{
						for (int32 NextFaceIndex = 0; NextFaceIndex < RelevantFacesForCorner[CornerIndex].Num(); NextFaceIndex++)
						{
							FFanFace& NextFace = RelevantFacesForCorner[CornerIndex][NextFaceIndex];
							if (!NextFace.bFilled) // && !NextFace.bBlendTangents)
							{
								if ((NextFaceIndex != OtherFaceIdx)
									&& (SmoothingGroupIndices[NextFace.FaceIndex] & SmoothingGroupIndices[OtherFace.FaceIndex]))
								{
									int32 CommonVertices = 0;
									int32 CommonTangentVertices = 0;
									int32 CommonNormalVertices = 0;
									for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
									{
										for (int32 NextCornerIndex = 0; NextCornerIndex < 3; NextCornerIndex++)
										{
											int32 NextVertexIndex = InIndices[NextFace.FaceIndex * 3 + NextCornerIndex];
											int32 OtherVertexIndex = InIndices[OtherFace.FaceIndex * 3 + OtherCornerIndex];
											if (PointsEqual(
												InVertices[NextVertexIndex],
												InVertices[OtherVertexIndex],
												ComparisonThreshold))
											{
												CommonVertices++;


												const FVector2D& UVOne = InUVs[NextFace.FaceIndex * 3 + NextCornerIndex];
												const FVector2D& UVTwo = InUVs[OtherFace.FaceIndex * 3 + OtherCornerIndex];

												if (UVsEqual(UVOne, UVTwo))
												{
													CommonTangentVertices++;
												}
												if (bBlendOverlappingNormals
													|| NextVertexIndex == OtherVertexIndex)
												{
													CommonNormalVertices++;
												}
											}
										}
									}
									// Flood fill faces with more than one common vertices which must be touching edges.
									if (CommonVertices > 1)
									{
										NextFace.bFilled = true;
										NextFace.bBlendNormals = (CommonNormalVertices > 1);
										NewConnections++;

										// Only blend tangents if there is no UV seam along the edge with this face.
										if (OtherFace.bBlendTangents && CommonTangentVertices > 1)
										{
											float OtherDeterminant = FVector::Triple(
												TriangleTangentX[NextFace.FaceIndex],
												TriangleTangentY[NextFace.FaceIndex],
												TriangleTangentZ[NextFace.FaceIndex]
												);
											if ((Determinant * OtherDeterminant) > 0.0f)
											{
												NextFace.bBlendTangents = true;
											}
										}
									}
								}
							}
						}
					}
				}
			} while (NewConnections > 0);
		}

		// Vertex normal construction.
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			if (bCornerHasTangents[CornerIndex])
			{
				CornerTangentX[CornerIndex] = OutTangentX[WedgeOffset + CornerIndex];
				CornerTangentY[CornerIndex] = OutTangentY[WedgeOffset + CornerIndex];
				CornerTangentZ[CornerIndex] = OutTangentZ[WedgeOffset + CornerIndex];
			}
			else
			{
				for (int32 RelevantFaceIdx = 0; RelevantFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); RelevantFaceIdx++)
				{
					FFanFace const& RelevantFace = RelevantFacesForCorner[CornerIndex][RelevantFaceIdx];
					if (RelevantFace.bFilled)
					{
						int32 OtherFaceIndex = RelevantFace.FaceIndex;
						if (RelevantFace.bBlendTangents)
						{
							CornerTangentX[CornerIndex] += TriangleTangentX[OtherFaceIndex];
							CornerTangentY[CornerIndex] += TriangleTangentY[OtherFaceIndex];
						}
						if (RelevantFace.bBlendNormals)
						{
							CornerTangentZ[CornerIndex] += TriangleTangentZ[OtherFaceIndex];
						}
					}
				}
				if (!OutTangentX[WedgeOffset + CornerIndex].IsZero())
				{
					CornerTangentX[CornerIndex] = OutTangentX[WedgeOffset + CornerIndex];
				}
				if (!OutTangentY[WedgeOffset + CornerIndex].IsZero())
				{
					CornerTangentY[CornerIndex] = OutTangentY[WedgeOffset + CornerIndex];
				}
				if (!OutTangentZ[WedgeOffset + CornerIndex].IsZero())
				{
					CornerTangentZ[CornerIndex] = OutTangentZ[WedgeOffset + CornerIndex];
				}
			}
		}

		// Normalization.
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			CornerTangentX[CornerIndex].Normalize();
			CornerTangentY[CornerIndex].Normalize();
			CornerTangentZ[CornerIndex].Normalize();

			// Gram-Schmidt orthogonalization
			CornerTangentY[CornerIndex] -= CornerTangentX[CornerIndex] * (CornerTangentX[CornerIndex] | CornerTangentY[CornerIndex]);
			CornerTangentY[CornerIndex].Normalize();

			CornerTangentX[CornerIndex] -= CornerTangentZ[CornerIndex] * (CornerTangentZ[CornerIndex] | CornerTangentX[CornerIndex]);
			CornerTangentX[CornerIndex].Normalize();
			CornerTangentY[CornerIndex] -= CornerTangentZ[CornerIndex] * (CornerTangentZ[CornerIndex] | CornerTangentY[CornerIndex]);
			CornerTangentY[CornerIndex].Normalize();
		}

		// Copy back to the mesh.
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			OutTangentX[WedgeOffset + CornerIndex] = CornerTangentX[CornerIndex];
			OutTangentY[WedgeOffset + CornerIndex] = CornerTangentY[CornerIndex];
			OutTangentZ[WedgeOffset + CornerIndex] = CornerTangentZ[CornerIndex];
		}
	}

	check(OutTangentX.Num() == NumWedges);
	check(OutTangentY.Num() == NumWedges);
	check(OutTangentZ.Num() == NumWedges);
}


static void ComputeTangents(
	FRawMesh& RawMesh,
	TMultiMap<int32, int32> const& OverlappingCorners,
	uint32 TangentOptions
	)
{
	const float ComparisonThreshold = (TangentOptions & ETangentOptions::IgnoreDegenerateTriangles) ? THRESH_POINTS_ARE_SAME : 0.0f;
	ComputeTangents(RawMesh.VertexPositions, RawMesh.WedgeIndices, RawMesh.WedgeTexCoords[0], RawMesh.FaceSmoothingMasks, OverlappingCorners, RawMesh.WedgeTangentX, RawMesh.WedgeTangentY, RawMesh.WedgeTangentZ, TangentOptions);
}

/*------------------------------------------------------------------------------
MikkTSpace for computing tangents.
------------------------------------------------------------------------------*/

static int MikkGetNumFaces(const SMikkTSpaceContext* Context)
{
	FRawMesh *UserData = (FRawMesh*)(Context->m_pUserData);
	return UserData->WedgeIndices.Num() / 3;
}

static int MikkGetNumVertsOfFace(const SMikkTSpaceContext* Context, const int FaceIdx)
{
	// All of our meshes are triangles.
	return 3;
}

static void MikkGetPosition(const SMikkTSpaceContext* Context, float Position[3], const int FaceIdx, const int VertIdx)
{
	FRawMesh *UserData = (FRawMesh*)(Context->m_pUserData);
	FVector VertexPosition = UserData->GetWedgePosition(FaceIdx * 3 + VertIdx);
	Position[0] = VertexPosition.X;
	Position[1] = VertexPosition.Y;
	Position[2] = VertexPosition.Z;
}

static void MikkGetNormal(const SMikkTSpaceContext* Context, float Normal[3], const int FaceIdx, const int VertIdx)
{
	FRawMesh *UserData = (FRawMesh*)(Context->m_pUserData);
	FVector &VertexNormal = UserData->WedgeTangentZ[FaceIdx * 3 + VertIdx];
	for (int32 i = 0; i < 3; ++i)
	{
		Normal[i] = VertexNormal[i];
	}
}

static void MikkSetTSpaceBasic(const SMikkTSpaceContext* Context, const float Tangent[3], const float BitangentSign, const int FaceIdx, const int VertIdx)
{
	FRawMesh *UserData = (FRawMesh*)(Context->m_pUserData);
	FVector &VertexTangent = UserData->WedgeTangentX[FaceIdx * 3 + VertIdx];
	for (int32 i = 0; i < 3; ++i)
	{
		VertexTangent[i] = Tangent[i];
	}
	FVector Bitangent = BitangentSign * FVector::CrossProduct(UserData->WedgeTangentZ[FaceIdx * 3 + VertIdx], VertexTangent);
	FVector &VertexBitangent = UserData->WedgeTangentY[FaceIdx * 3 + VertIdx];
	for (int32 i = 0; i < 3; ++i)
	{
		VertexBitangent[i] = -Bitangent[i];
	}
}

static void MikkGetTexCoord(const SMikkTSpaceContext* Context, float UV[2], const int FaceIdx, const int VertIdx)
{
	FRawMesh *UserData = (FRawMesh*)(Context->m_pUserData);
	FVector2D &TexCoord = UserData->WedgeTexCoords[0][FaceIdx * 3 + VertIdx];
	UV[0] = TexCoord.X;
	UV[1] = TexCoord.Y;
}

// MikkTSpace implementations for skeletal meshes, where tangents/bitangents are ultimately derived from lists of attributes.

// Holder for skeletal data to be passed to MikkTSpace.
// Holds references to the wedge, face and points vectors that BuildSkeletalMesh is given.
// Holds reference to the calculated normals array, which will be fleshed out if they've been calculated.
// Holds reference to the newly created tangent and bitangent arrays, which MikkTSpace will fleshed out if required.
class MikkTSpace_Skeletal_Mesh
{
public:
	const TArray<FMeshWedge>	&wedges;			//Reference to wedge list.
	const TArray<FMeshFace>		&faces;				//Reference to face list.	Also contains normal/tangent/bitanget/UV coords for each vertex of the face.
	const TArray<FVector>		&points;			//Reference to position list.
	bool						bComputeNormals;	//Copy of bComputeNormals.
	TArray<FVector>				&TangentsX;			//Reference to newly created tangents list.
	TArray<FVector>				&TangentsY;			//Reference to newly created bitangents list.
	TArray<FVector>				&TangentsZ;			//Reference to computed normals, will be empty otherwise.

	MikkTSpace_Skeletal_Mesh(
		const TArray<FMeshWedge>	&Wedges,
		const TArray<FMeshFace>		&Faces,
		const TArray<FVector>		&Points,
		bool						bInComputeNormals,
		TArray<FVector>				&VertexTangentsX,
		TArray<FVector>				&VertexTangentsY,
		TArray<FVector>				&VertexTangentsZ
		)
		:
		wedges(Wedges),
		faces(Faces),
		points(Points),
		bComputeNormals(bInComputeNormals),
		TangentsX(VertexTangentsX),
		TangentsY(VertexTangentsY),
		TangentsZ(VertexTangentsZ)
	{
	}
};

static int MikkGetNumFaces_Skeletal(const SMikkTSpaceContext* Context)
{
	MikkTSpace_Skeletal_Mesh *UserData = (MikkTSpace_Skeletal_Mesh*)(Context->m_pUserData);
	return UserData->faces.Num();
}

static int MikkGetNumVertsOfFace_Skeletal(const SMikkTSpaceContext* Context, const int FaceIdx)
{
	// Confirmed?
	return 3;
}

static void MikkGetPosition_Skeletal(const SMikkTSpaceContext* Context, float Position[3], const int FaceIdx, const int VertIdx)
{
	MikkTSpace_Skeletal_Mesh *UserData = (MikkTSpace_Skeletal_Mesh*)(Context->m_pUserData);
	const FVector &VertexPosition = UserData->points[UserData->wedges[UserData->faces[FaceIdx].iWedge[VertIdx]].iVertex];
	Position[0] = VertexPosition.X;
	Position[1] = VertexPosition.Y;
	Position[2] = VertexPosition.Z;
}

static void MikkGetNormal_Skeletal(const SMikkTSpaceContext* Context, float Normal[3], const int FaceIdx, const int VertIdx)
{
	MikkTSpace_Skeletal_Mesh *UserData = (MikkTSpace_Skeletal_Mesh*)(Context->m_pUserData);
	// Get different normals depending on whether they've been calculated or not.
	if (UserData->bComputeNormals) {
		FVector &VertexNormal = UserData->TangentsZ[FaceIdx * 3 + VertIdx];
		Normal[0] = VertexNormal.X;
		Normal[1] = VertexNormal.Y;
		Normal[2] = VertexNormal.Z;
	}
	else
	{
		const FVector &VertexNormal = UserData->faces[FaceIdx].TangentZ[VertIdx];
		Normal[0] = VertexNormal.X;
		Normal[1] = VertexNormal.Y;
		Normal[2] = VertexNormal.Z;
	}
}

static void MikkSetTSpaceBasic_Skeletal(const SMikkTSpaceContext* Context, const float Tangent[3], const float BitangentSign, const int FaceIdx, const int VertIdx)
{
	MikkTSpace_Skeletal_Mesh *UserData = (MikkTSpace_Skeletal_Mesh*)(Context->m_pUserData);
	FVector &VertexTangent = UserData->TangentsX[FaceIdx * 3 + VertIdx];
	VertexTangent.X = Tangent[0];
	VertexTangent.Y = Tangent[1];
	VertexTangent.Z = Tangent[2];

	FVector Bitangent;
	// Get different normals depending on whether they've been calculated or not.
	if (UserData->bComputeNormals) {
		Bitangent = BitangentSign * FVector::CrossProduct(UserData->TangentsZ[FaceIdx * 3 + VertIdx], VertexTangent);
	}
	else
	{
		Bitangent = BitangentSign * FVector::CrossProduct(UserData->faces[FaceIdx].TangentZ[VertIdx], VertexTangent);
	}
	FVector &VertexBitangent = UserData->TangentsY[FaceIdx * 3 + VertIdx];
	// Switch the tangent space swizzle to X+Y-Z+ for legacy reasons.
	VertexBitangent.X = -Bitangent[0];
	VertexBitangent.Y = -Bitangent[1];
	VertexBitangent.Z = -Bitangent[2];
}

static void MikkGetTexCoord_Skeletal(const SMikkTSpaceContext* Context, float UV[2], const int FaceIdx, const int VertIdx)
{
	MikkTSpace_Skeletal_Mesh *UserData = (MikkTSpace_Skeletal_Mesh*)(Context->m_pUserData);
	const FVector2D &TexCoord = UserData->wedges[UserData->faces[FaceIdx].iWedge[VertIdx]].UVs[0];
	UV[0] = TexCoord.X;
	UV[1] = TexCoord.Y;
}

static void ComputeTangents_MikkTSpace(
	FRawMesh& RawMesh,
	TMultiMap<int32, int32> const& OverlappingCorners,
	uint32 TangentOptions
	)
{
	bool bBlendOverlappingNormals = (TangentOptions & ETangentOptions::BlendOverlappingNormals) != 0;
	bool bIgnoreDegenerateTriangles = (TangentOptions & ETangentOptions::IgnoreDegenerateTriangles) != 0;
	float ComparisonThreshold = bIgnoreDegenerateTriangles ? THRESH_POINTS_ARE_SAME : 0.0f;

	// Compute per-triangle tangents.
	TArray<FVector> TriangleTangentX;
	TArray<FVector> TriangleTangentY;
	TArray<FVector> TriangleTangentZ;

	ComputeTriangleTangents(
		TriangleTangentX,
		TriangleTangentY,
		TriangleTangentZ,
		RawMesh,
		bIgnoreDegenerateTriangles ? SMALL_NUMBER : 0.0f
		);

	// Declare these out here to avoid reallocations.
	TArray<FFanFace> RelevantFacesForCorner[3];
	TArray<int32> AdjacentFaces;
	TArray<int32> DupVerts;

	int32 NumWedges = RawMesh.WedgeIndices.Num();
	int32 NumFaces = NumWedges / 3;

	bool bWedgeNormals = true;
	bool bWedgeTSpace = false;
	for (int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeTangentZ.Num(); ++WedgeIdx)
	{
		bWedgeNormals = bWedgeNormals && (!RawMesh.WedgeTangentZ[WedgeIdx].IsNearlyZero());
	}

	if (RawMesh.WedgeTangentX.Num() > 0 && RawMesh.WedgeTangentY.Num() > 0)
	{
		bWedgeTSpace = true;
		for (int32 WedgeIdx = 0; WedgeIdx < RawMesh.WedgeTangentX.Num()
			&& WedgeIdx < RawMesh.WedgeTangentY.Num(); ++WedgeIdx)
		{
			bWedgeTSpace = bWedgeTSpace && (!RawMesh.WedgeTangentX[WedgeIdx].IsNearlyZero()) && (!RawMesh.WedgeTangentY[WedgeIdx].IsNearlyZero());
		}
	}

	// Allocate storage for tangents if none were provided, and calculate normals for MikkTSpace.
	if (RawMesh.WedgeTangentZ.Num() != NumWedges || !bWedgeNormals)
	{
		// normals are not included, so we should calculate them
		RawMesh.WedgeTangentZ.Empty(NumWedges);
		RawMesh.WedgeTangentZ.AddZeroed(NumWedges);

		// we need to calculate normals for MikkTSpace
		UE_LOG(LogMeshUtilities, Log, TEXT("Invalid vertex normals found for mesh. Forcing recomputation of vertex normals for MikkTSpace. Fix mesh or disable \"Use MikkTSpace Tangent Space\" to avoid forced recomputation of normals."));

		for (int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
		{
			int32 WedgeOffset = FaceIndex * 3;
			FVector CornerPositions[3];
			FVector CornerNormal[3];

			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				CornerNormal[CornerIndex] = FVector::ZeroVector;
				CornerPositions[CornerIndex] = GetPositionForWedge(RawMesh, WedgeOffset + CornerIndex);
				RelevantFacesForCorner[CornerIndex].Reset();
			}

			// Don't process degenerate triangles.
			if (PointsEqual(CornerPositions[0], CornerPositions[1], ComparisonThreshold)
				|| PointsEqual(CornerPositions[0], CornerPositions[2], ComparisonThreshold)
				|| PointsEqual(CornerPositions[1], CornerPositions[2], ComparisonThreshold))
			{
				continue;
			}

			// No need to process triangles if tangents already exist.
			bool bCornerHasNormal[3] = { 0 };
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				bCornerHasNormal[CornerIndex] = !RawMesh.WedgeTangentZ[WedgeOffset + CornerIndex].IsZero();
			}
			if (bCornerHasNormal[0] && bCornerHasNormal[1] && bCornerHasNormal[2])
			{
				continue;
			}

			// Start building a list of faces adjacent to this face.
			AdjacentFaces.Reset();
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				int32 ThisCornerIndex = WedgeOffset + CornerIndex;
				DupVerts.Reset();
				OverlappingCorners.MultiFind(ThisCornerIndex, DupVerts);
				DupVerts.Add(ThisCornerIndex); // I am a "dup" of myself
				for (int32 k = 0; k < DupVerts.Num(); k++)
				{
					AdjacentFaces.AddUnique(DupVerts[k] / 3);
				}
			}

			// We need to sort these here because the criteria for point equality is
			// exact, so we must ensure the exact same order for all dups.
			AdjacentFaces.Sort();

			// Process adjacent faces
			for (int32 AdjacentFaceIndex = 0; AdjacentFaceIndex < AdjacentFaces.Num(); AdjacentFaceIndex++)
			{
				int32 OtherFaceIndex = AdjacentFaces[AdjacentFaceIndex];
				for (int32 OurCornerIndex = 0; OurCornerIndex < 3; OurCornerIndex++)
				{
					if (bCornerHasNormal[OurCornerIndex])
						continue;

					FFanFace NewFanFace;
					int32 CommonIndexCount = 0;

					// Check for vertices in common.
					if (FaceIndex == OtherFaceIndex)
					{
						CommonIndexCount = 3;
						NewFanFace.LinkedVertexIndex = OurCornerIndex;
					}
					else
					{
						// Check matching vertices against main vertex .
						for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
						{
							if (PointsEqual(
								CornerPositions[OurCornerIndex],
								GetPositionForWedge(RawMesh, OtherFaceIndex * 3 + OtherCornerIndex),
								ComparisonThreshold
								))
							{
								CommonIndexCount++;
								NewFanFace.LinkedVertexIndex = OtherCornerIndex;
							}
						}
					}

					// Add if connected by at least one point. Smoothing matches are considered later.
					if (CommonIndexCount > 0)
					{
						NewFanFace.FaceIndex = OtherFaceIndex;
						NewFanFace.bFilled = (OtherFaceIndex == FaceIndex); // Starter face for smoothing floodfill.
						NewFanFace.bBlendTangents = NewFanFace.bFilled;
						NewFanFace.bBlendNormals = NewFanFace.bFilled;
						RelevantFacesForCorner[OurCornerIndex].Add(NewFanFace);
					}
				}
			}

			// Find true relevance of faces for a vertex normal by traversing
			// smoothing-group-compatible connected triangle fans around common vertices.
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				if (bCornerHasNormal[CornerIndex])
					continue;

				int32 NewConnections;
				do
				{
					NewConnections = 0;
					for (int32 OtherFaceIdx = 0; OtherFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); OtherFaceIdx++)
					{
						FFanFace& OtherFace = RelevantFacesForCorner[CornerIndex][OtherFaceIdx];
						// The vertex' own face is initially the only face with bFilled == true.
						if (OtherFace.bFilled)
						{
							for (int32 NextFaceIndex = 0; NextFaceIndex < RelevantFacesForCorner[CornerIndex].Num(); NextFaceIndex++)
							{
								FFanFace& NextFace = RelevantFacesForCorner[CornerIndex][NextFaceIndex];
								if (!NextFace.bFilled) // && !NextFace.bBlendTangents)
								{
									if ((NextFaceIndex != OtherFaceIdx)
										&& (RawMesh.FaceSmoothingMasks[NextFace.FaceIndex] & RawMesh.FaceSmoothingMasks[OtherFace.FaceIndex]))
									{
										int32 CommonVertices = 0;
										int32 CommonNormalVertices = 0;
										for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
										{
											for (int32 NextCornerIndex = 0; NextCornerIndex < 3; NextCornerIndex++)
											{
												int32 NextVertexIndex = RawMesh.WedgeIndices[NextFace.FaceIndex * 3 + NextCornerIndex];
												int32 OtherVertexIndex = RawMesh.WedgeIndices[OtherFace.FaceIndex * 3 + OtherCornerIndex];
												if (PointsEqual(
													RawMesh.VertexPositions[NextVertexIndex],
													RawMesh.VertexPositions[OtherVertexIndex],
													ComparisonThreshold))
												{
													CommonVertices++;
													if (bBlendOverlappingNormals
														|| NextVertexIndex == OtherVertexIndex)
													{
														CommonNormalVertices++;
													}
												}
											}
										}
										// Flood fill faces with more than one common vertices which must be touching edges.
										if (CommonVertices > 1)
										{
											NextFace.bFilled = true;
											NextFace.bBlendNormals = (CommonNormalVertices > 1);
											NewConnections++;
										}
									}
								}
							}
						}
					}
				} 
				while (NewConnections > 0);
			}


			// Vertex normal construction.
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				if (bCornerHasNormal[CornerIndex])
				{
					CornerNormal[CornerIndex] = RawMesh.WedgeTangentZ[WedgeOffset + CornerIndex];
				}
				else
				{
					for (int32 RelevantFaceIdx = 0; RelevantFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); RelevantFaceIdx++)
					{
						FFanFace const& RelevantFace = RelevantFacesForCorner[CornerIndex][RelevantFaceIdx];
						if (RelevantFace.bFilled)
						{
							int32 OtherFaceIndex = RelevantFace.FaceIndex;
							if (RelevantFace.bBlendNormals)
							{
								CornerNormal[CornerIndex] += TriangleTangentZ[OtherFaceIndex];
							}
						}
					}
					if (!RawMesh.WedgeTangentZ[WedgeOffset + CornerIndex].IsZero())
					{
						CornerNormal[CornerIndex] = RawMesh.WedgeTangentZ[WedgeOffset + CornerIndex];
					}
				}
			}

			// Normalization.
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				CornerNormal[CornerIndex].Normalize();
			}

			// Copy back to the mesh.
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				RawMesh.WedgeTangentZ[WedgeOffset + CornerIndex] = CornerNormal[CornerIndex];
			}
		}
	}

	if (RawMesh.WedgeTangentX.Num() != NumWedges)
	{
		RawMesh.WedgeTangentX.Empty(NumWedges);
		RawMesh.WedgeTangentX.AddZeroed(NumWedges);
	}
	if (RawMesh.WedgeTangentY.Num() != NumWedges)
	{
		RawMesh.WedgeTangentY.Empty(NumWedges);
		RawMesh.WedgeTangentY.AddZeroed(NumWedges);
	}

	if (!bWedgeTSpace)
	{
		// we can use mikktspace to calculate the tangents
		SMikkTSpaceInterface MikkTInterface;
		MikkTInterface.m_getNormal = MikkGetNormal;
		MikkTInterface.m_getNumFaces = MikkGetNumFaces;
		MikkTInterface.m_getNumVerticesOfFace = MikkGetNumVertsOfFace;
		MikkTInterface.m_getPosition = MikkGetPosition;
		MikkTInterface.m_getTexCoord = MikkGetTexCoord;
		MikkTInterface.m_setTSpaceBasic = MikkSetTSpaceBasic;
		MikkTInterface.m_setTSpace = nullptr;

		SMikkTSpaceContext MikkTContext;
		MikkTContext.m_pInterface = &MikkTInterface;
		MikkTContext.m_pUserData = (void*)(&RawMesh);
		MikkTContext.m_bIgnoreDegenerates = bIgnoreDegenerateTriangles;
		genTangSpaceDefault(&MikkTContext);
	}

	check(RawMesh.WedgeTangentX.Num() == NumWedges);
	check(RawMesh.WedgeTangentY.Num() == NumWedges);
	check(RawMesh.WedgeTangentZ.Num() == NumWedges);
}

static void ComputeStreamingTextureFactors(
	float* OutStreamingTextureFactors,
	float* OutMaxStreamingTextureFactor,
	const FRawMesh& Mesh,
	const FVector& BuildScale
	)
{
	int32 NumTexCoords = ComputeNumTexCoords(Mesh, MAX_STATIC_TEXCOORDS);
	int32 NumFaces = Mesh.WedgeIndices.Num() / 3;
	TArray<float> TexelRatios[MAX_STATIC_TEXCOORDS];
	float MaxStreamingTextureFactor = 0.0f;
	for (int32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		int32 Wedge0 = FaceIndex * 3 + 0;
		int32 Wedge1 = FaceIndex * 3 + 1;
		int32 Wedge2 = FaceIndex * 3 + 2;

		const FVector& Pos0 = Mesh.GetWedgePosition(Wedge0) * BuildScale;
		const FVector& Pos1 = Mesh.GetWedgePosition(Wedge1) * BuildScale;
		const FVector& Pos2 = Mesh.GetWedgePosition(Wedge2) * BuildScale;
		float	L1 = (Pos0 - Pos1).Size(),
			L2 = (Pos0 - Pos2).Size();

		for (int32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++)
		{
			FVector2D UV0 = Mesh.WedgeTexCoords[UVIndex][Wedge0];
			FVector2D UV1 = Mesh.WedgeTexCoords[UVIndex][Wedge1];
			FVector2D UV2 = Mesh.WedgeTexCoords[UVIndex][Wedge2];

			float	T1 = (UV0 - UV1).Size(),
				T2 = (UV0 - UV2).Size();

			if (FMath::Abs(T1 * T2) > FMath::Square(SMALL_NUMBER))
			{
				const float TexelRatio = FMath::Max(L1 / T1, L2 / T2);
				TexelRatios[UVIndex].Add(TexelRatio);

				// Update max texel ratio
				if (TexelRatio > MaxStreamingTextureFactor)
				{
					MaxStreamingTextureFactor = TexelRatio;
				}
			}
		}
	}

	for (int32 UVIndex = 0; UVIndex < MAX_STATIC_TEXCOORDS; UVIndex++)
	{
		OutStreamingTextureFactors[UVIndex] = 0.0f;
		if (TexelRatios[UVIndex].Num())
		{
			// Disregard upper 75% of texel ratios.
			// This is to ignore backfacing surfaces or other non-visible surfaces that tend to map a small number of texels to a large surface.
			TexelRatios[UVIndex].Sort(TGreater<float>());
			float TexelRatio = TexelRatios[UVIndex][FMath::TruncToInt(TexelRatios[UVIndex].Num() * 0.75f)];
			OutStreamingTextureFactors[UVIndex] = TexelRatio;
		}
	}
	*OutMaxStreamingTextureFactor = MaxStreamingTextureFactor;
}

static void BuildDepthOnlyIndexBuffer(
	TArray<uint32>& OutDepthIndices,
	const TArray<FStaticMeshBuildVertex>& InVertices,
	const TArray<uint32>& InIndices,
	const TArray<FStaticMeshSection>& InSections
	)
{
	int32 NumVertices = InVertices.Num();
	if (InIndices.Num() <= 0 || NumVertices <= 0)
	{
		OutDepthIndices.Empty();
		return;
	}

	// Create a mapping of index -> first overlapping index to accelerate the construction of the shadow index buffer.
	TArray<FIndexAndZ> VertIndexAndZ;
	VertIndexAndZ.Empty(NumVertices);
	for (int32 VertIndex = 0; VertIndex < NumVertices; VertIndex++)
	{
		new(VertIndexAndZ)FIndexAndZ(VertIndex, InVertices[VertIndex].Position);
	}
	VertIndexAndZ.Sort(FCompareIndexAndZ());

	// Setup the index map. 0xFFFFFFFF == not set.
	TArray<uint32> IndexMap;
	IndexMap.AddUninitialized(NumVertices);
	FMemory::Memset(IndexMap.GetData(), 0xFF, NumVertices * sizeof(uint32));

	// Search for duplicates, quickly!
	for (int32 i = 0; i < VertIndexAndZ.Num(); i++)
	{
		uint32 SrcIndex = VertIndexAndZ[i].Index;
		float Z = VertIndexAndZ[i].Z;
		IndexMap[SrcIndex] = FMath::Min(IndexMap[SrcIndex], SrcIndex);

		// Search forward since we add pairs both ways.
		for (int32 j = i + 1; j < VertIndexAndZ.Num(); j++)
		{
			if (FMath::Abs(VertIndexAndZ[j].Z - Z) > THRESH_POINTS_ARE_SAME * 4.01f)
				break; // can't be any more dups

			uint32 OtherIndex = VertIndexAndZ[j].Index;
			if (PointsEqual(InVertices[SrcIndex].Position, InVertices[OtherIndex].Position,/*bUseEpsilonCompare=*/ true))
			{
				IndexMap[SrcIndex] = FMath::Min(IndexMap[SrcIndex], OtherIndex);
				IndexMap[OtherIndex] = FMath::Min(IndexMap[OtherIndex], SrcIndex);
			}
		}
	}

	// Build the depth-only index buffer by remapping all indices to the first overlapping
	// vertex in the vertex buffer.
	OutDepthIndices.Empty();
	for (int32 SectionIndex = 0; SectionIndex < InSections.Num(); ++SectionIndex)
	{
		const FStaticMeshSection& Section = InSections[SectionIndex];
		int32 FirstIndex = Section.FirstIndex;
		int32 LastIndex = FirstIndex + Section.NumTriangles * 3;
		for (int32 SrcIndex = FirstIndex; SrcIndex < LastIndex; ++SrcIndex)
		{
			uint32 VertIndex = InIndices[SrcIndex];
			OutDepthIndices.Add(IndexMap[VertIndex]);
		}
	}
}

static float GetComparisonThreshold(FMeshBuildSettings const& BuildSettings)
{
	return BuildSettings.bRemoveDegenerates ? THRESH_POINTS_ARE_SAME : 0.0f;
}

/*------------------------------------------------------------------------------
Static mesh building.
------------------------------------------------------------------------------*/

static FStaticMeshBuildVertex BuildStaticMeshVertex(FRawMesh const& RawMesh, int32 WedgeIndex, FVector BuildScale)
{
	FStaticMeshBuildVertex Vertex;
	Vertex.Position = GetPositionForWedge(RawMesh, WedgeIndex) * BuildScale;

	const FMatrix ScaleMatrix = FScaleMatrix(BuildScale).Inverse().GetTransposed();
	Vertex.TangentX = ScaleMatrix.TransformVector(RawMesh.WedgeTangentX[WedgeIndex]).GetSafeNormal();
	Vertex.TangentY = ScaleMatrix.TransformVector(RawMesh.WedgeTangentY[WedgeIndex]).GetSafeNormal();
	Vertex.TangentZ = ScaleMatrix.TransformVector(RawMesh.WedgeTangentZ[WedgeIndex]).GetSafeNormal();

	if (RawMesh.WedgeColors.IsValidIndex(WedgeIndex))
	{
		Vertex.Color = RawMesh.WedgeColors[WedgeIndex];
	}
	else
	{
		Vertex.Color = FColor::White;
	}

	int32 NumTexCoords = FMath::Min<int32>(MAX_MESH_TEXTURE_COORDS, MAX_STATIC_TEXCOORDS);
	for (int32 i = 0; i < NumTexCoords; ++i)
	{
		if (RawMesh.WedgeTexCoords[i].IsValidIndex(WedgeIndex))
		{
			Vertex.UVs[i] = RawMesh.WedgeTexCoords[i][WedgeIndex];
		}
		else
		{
			Vertex.UVs[i] = FVector2D(0.0f, 0.0f);
		}
	}
	return Vertex;
}

static bool AreVerticesEqual(
	FStaticMeshBuildVertex const& A,
	FStaticMeshBuildVertex const& B,
	float ComparisonThreshold
	)
{
	if (!PointsEqual(A.Position, B.Position, ComparisonThreshold)
		|| !NormalsEqual(A.TangentX, B.TangentX)
		|| !NormalsEqual(A.TangentY, B.TangentY)
		|| !NormalsEqual(A.TangentZ, B.TangentZ)
		|| A.Color != B.Color)
	{
		return false;
	}

	// UVs
	for (int32 UVIndex = 0; UVIndex < MAX_STATIC_TEXCOORDS; UVIndex++)
	{
		if (!UVsEqual(A.UVs[UVIndex], B.UVs[UVIndex]))
		{
			return false;
		}
	}

	return true;
}

static void BuildStaticMeshVertexAndIndexBuffers(
	TArray<FStaticMeshBuildVertex>& OutVertices,
	TArray<TArray<uint32> >& OutPerSectionIndices,
	TArray<int32>& OutWedgeMap,
	const FRawMesh& RawMesh,
	const TMultiMap<int32, int32>& OverlappingCorners,
	float ComparisonThreshold,
	FVector BuildScale
	)
{
	TMap<int32, int32> FinalVerts;
	TArray<int32> DupVerts;
	int32 NumFaces = RawMesh.WedgeIndices.Num() / 3;

	// Process each face, build vertex buffer and per-section index buffers.
	for (int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
	{
		int32 VertexIndices[3];
		FVector CornerPositions[3];

		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			CornerPositions[CornerIndex] = GetPositionForWedge(RawMesh, FaceIndex * 3 + CornerIndex);
		}

		// Don't process degenerate triangles.
		if (PointsEqual(CornerPositions[0], CornerPositions[1], ComparisonThreshold)
			|| PointsEqual(CornerPositions[0], CornerPositions[2], ComparisonThreshold)
			|| PointsEqual(CornerPositions[1], CornerPositions[2], ComparisonThreshold))
		{
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				OutWedgeMap.Add(INDEX_NONE);
			}
			continue;
		}

		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			int32 WedgeIndex = FaceIndex * 3 + CornerIndex;
			FStaticMeshBuildVertex ThisVertex = BuildStaticMeshVertex(RawMesh, WedgeIndex, BuildScale);

			DupVerts.Reset();
			OverlappingCorners.MultiFind(WedgeIndex, DupVerts);
			DupVerts.Sort();

			int32 Index = INDEX_NONE;
			for (int32 k = 0; k < DupVerts.Num(); k++)
			{
				if (DupVerts[k] >= WedgeIndex)
				{
					// the verts beyond me haven't been placed yet, so these duplicates are not relevant
					break;
				}

				int32 *Location = FinalVerts.Find(DupVerts[k]);
				if (Location != NULL
					&& AreVerticesEqual(ThisVertex, OutVertices[*Location], ComparisonThreshold))
				{
					Index = *Location;
					break;
				}
			}
			if (Index == INDEX_NONE)
			{
				Index = OutVertices.Add(ThisVertex);
				FinalVerts.Add(WedgeIndex, Index);
			}
			VertexIndices[CornerIndex] = Index;
		}

		// Reject degenerate triangles.
		if (VertexIndices[0] == VertexIndices[1]
			|| VertexIndices[1] == VertexIndices[2]
			|| VertexIndices[0] == VertexIndices[2])
		{
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				OutWedgeMap.Add(INDEX_NONE);
			}
			continue;
		}

		// Put the indices in the material index buffer.
		int32 SectionIndex = FMath::Clamp(RawMesh.FaceMaterialIndices[FaceIndex], 0, OutPerSectionIndices.Num() - 1);
		TArray<uint32>& SectionIndices = OutPerSectionIndices[SectionIndex];
		for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
		{
			SectionIndices.Add(VertexIndices[CornerIndex]);
			OutWedgeMap.Add(VertexIndices[CornerIndex]);
		}
	}
}

void FMeshUtilities::CacheOptimizeVertexAndIndexBuffer(
	TArray<FStaticMeshBuildVertex>& Vertices,
	TArray<TArray<uint32> >& PerSectionIndices,
	TArray<int32>& WedgeMap
	)
{
	// Copy the vertices since we will be reordering them
	TArray<FStaticMeshBuildVertex> OriginalVertices = Vertices;

	// Initialize a cache that stores which indices have been assigned
	TArray<int32> IndexCache;
	IndexCache.AddUninitialized(Vertices.Num());
	FMemory::Memset(IndexCache.GetData(), INDEX_NONE, IndexCache.Num() * IndexCache.GetTypeSize());
	int32 NextAvailableIndex = 0;

	// Iterate through the section index buffers, 
	// Optimizing index order for the post transform cache (minimizes the number of vertices transformed), 
	// And vertex order for the pre transform cache (minimizes the amount of vertex data fetched by the GPU).
	for (int32 SectionIndex = 0; SectionIndex < PerSectionIndices.Num(); SectionIndex++)
	{
		TArray<uint32>& Indices = PerSectionIndices[SectionIndex];

		if (Indices.Num())
		{
			// Optimize the index buffer for the post transform cache with.
			CacheOptimizeIndexBuffer(Indices);

			// Copy the index buffer since we will be reordering it
			TArray<uint32> OriginalIndices = Indices;

			// Go through the indices and assign them new values that are coherent where possible
			for (int32 Index = 0; Index < Indices.Num(); Index++)
			{
				const int32 CachedIndex = IndexCache[OriginalIndices[Index]];

				if (CachedIndex == INDEX_NONE)
				{
					// No new index has been allocated for this existing index, assign a new one
					Indices[Index] = NextAvailableIndex;
					// Mark what this index has been assigned to
					IndexCache[OriginalIndices[Index]] = NextAvailableIndex;
					NextAvailableIndex++;
				}
				else
				{
					// Reuse an existing index assignment
					Indices[Index] = CachedIndex;
				}
				// Reorder the vertices based on the new index assignment
				Vertices[Indices[Index]] = OriginalVertices[OriginalIndices[Index]];
			}
		}
	}

	for (int32 i = 0; i < WedgeMap.Num(); i++)
	{
		int32 MappedIndex = WedgeMap[i];
		if (MappedIndex != INDEX_NONE)
		{
			WedgeMap[i] = IndexCache[MappedIndex];
		}
	}
}

class FStaticMeshUtilityBuilder
{
public:
	FStaticMeshUtilityBuilder() : Stage(EStage::Uninit), NumValidLODs(0) {}

	bool GatherSourceMeshesPerLOD(TArray<FStaticMeshSourceModel>& SourceModels, IMeshReduction* MeshReduction)
	{
		check(Stage == EStage::Uninit);

		// Gather source meshes for each LOD.
		for (int32 LODIndex = 0; LODIndex < SourceModels.Num(); ++LODIndex)
		{
			FStaticMeshSourceModel& SrcModel = SourceModels[LODIndex];
			FRawMesh& RawMesh = *new(LODMeshes)FRawMesh;
			TMultiMap<int32, int32>& OverlappingCorners = *new(LODOverlappingCorners)TMultiMap<int32, int32>;

			if (!SrcModel.RawMeshBulkData->IsEmpty())
			{
				SrcModel.RawMeshBulkData->LoadRawMesh(RawMesh);
				// Make sure the raw mesh is not irreparably malformed.
				if (!RawMesh.IsValidOrFixable())
				{
					UE_LOG(LogMeshUtilities, Error, TEXT("Raw mesh is corrupt for LOD%d."), LODIndex);
					return false;
				}
				LODBuildSettings[LODIndex] = SrcModel.BuildSettings;

				float ComparisonThreshold = GetComparisonThreshold(LODBuildSettings[LODIndex]);
				int32 NumWedges = RawMesh.WedgeIndices.Num();

				// Find overlapping corners to accelerate adjacency.
				FindOverlappingCorners(OverlappingCorners, RawMesh, ComparisonThreshold);

				// Figure out if we should recompute normals and tangents.
				bool bRecomputeNormals = SrcModel.BuildSettings.bRecomputeNormals || RawMesh.WedgeTangentZ.Num() != NumWedges;
				bool bRecomputeTangents = SrcModel.BuildSettings.bRecomputeTangents || RawMesh.WedgeTangentX.Num() != NumWedges || RawMesh.WedgeTangentY.Num() != NumWedges;

				// Dump normals and tangents if we are recomputing them.
				if (bRecomputeTangents)
				{
					RawMesh.WedgeTangentX.Empty(NumWedges);
					RawMesh.WedgeTangentX.AddZeroed(NumWedges);
					RawMesh.WedgeTangentY.Empty(NumWedges);
					RawMesh.WedgeTangentY.AddZeroed(NumWedges);
				}
				if (bRecomputeNormals)
				{
					RawMesh.WedgeTangentZ.Empty(NumWedges);
					RawMesh.WedgeTangentZ.AddZeroed(NumWedges);
				}

				// Compute any missing tangents.
				{
					// Static meshes always blend normals of overlapping corners.
					uint32 TangentOptions = ETangentOptions::BlendOverlappingNormals;
					if (SrcModel.BuildSettings.bRemoveDegenerates)
					{
						// If removing degenerate triangles, ignore them when computing tangents.
						TangentOptions |= ETangentOptions::IgnoreDegenerateTriangles;
					}

					//MikkTSpace should be use only when the user want to recompute the normals or tangents otherwise should always fallback on builtin
					if (SrcModel.BuildSettings.bUseMikkTSpace && (SrcModel.BuildSettings.bRecomputeNormals || SrcModel.BuildSettings.bRecomputeTangents))
					{
						ComputeTangents_MikkTSpace(RawMesh, OverlappingCorners, TangentOptions);
					}
					else
					{
						ComputeTangents(RawMesh, OverlappingCorners, TangentOptions);
					}
				}

				// At this point the mesh will have valid tangents.
				check(RawMesh.WedgeTangentX.Num() == NumWedges);
				check(RawMesh.WedgeTangentY.Num() == NumWedges);
				check(RawMesh.WedgeTangentZ.Num() == NumWedges);

				// Generate lightmap UVs
				if (SrcModel.BuildSettings.bGenerateLightmapUVs)
				{
					if (RawMesh.WedgeTexCoords[SrcModel.BuildSettings.SrcLightmapIndex].Num() == 0)
					{
						SrcModel.BuildSettings.SrcLightmapIndex = 0;
					}

					FLayoutUV Packer(&RawMesh, SrcModel.BuildSettings.SrcLightmapIndex, SrcModel.BuildSettings.DstLightmapIndex, SrcModel.BuildSettings.MinLightmapResolution);

					Packer.FindCharts(OverlappingCorners);
					bool bPackSuccess = Packer.FindBestPacking();
					if (bPackSuccess)
					{
						Packer.CommitPackedUVs();
					}
				}
				HasRawMesh[LODIndex] = true;
			}
			else if (LODIndex > 0 && MeshReduction)
			{
				// If a raw mesh is not explicitly provided, use the raw mesh of the
				// next highest LOD.
				RawMesh = LODMeshes[LODIndex - 1];
				OverlappingCorners = LODOverlappingCorners[LODIndex - 1];
				LODBuildSettings[LODIndex] = LODBuildSettings[LODIndex - 1];
				HasRawMesh[LODIndex] = false;
			}
		}
		check(LODMeshes.Num() == SourceModels.Num());
		check(LODOverlappingCorners.Num() == SourceModels.Num());

		// Bail if there is no raw mesh data from which to build a renderable mesh.
		if (LODMeshes.Num() == 0 || LODMeshes[0].WedgeIndices.Num() == 0)
		{
			return false;
		}

		Stage = EStage::Gathered;
		return true;
	}

	bool ReduceLODs(TArray<FStaticMeshSourceModel>& SourceModels, const FStaticMeshLODGroup& LODGroup, IMeshReduction* MeshReduction, bool& bOutWasReduced)
	{
		check(Stage == EStage::Gathered);

		// Reduce each LOD mesh according to its reduction settings.
		for (int32 LODIndex = 0; LODIndex < SourceModels.Num(); ++LODIndex)
		{
			const FStaticMeshSourceModel& SrcModel = SourceModels[LODIndex];
			FMeshReductionSettings ReductionSettings = LODGroup.GetSettings(SrcModel.ReductionSettings, LODIndex);
			LODMaxDeviation[NumValidLODs] = 0.0f;
			if (LODIndex != NumValidLODs)
			{
				LODBuildSettings[NumValidLODs] = LODBuildSettings[LODIndex];
				LODOverlappingCorners[NumValidLODs] = LODOverlappingCorners[LODIndex];
			}

			if (MeshReduction && (ReductionSettings.PercentTriangles < 1.0f || ReductionSettings.MaxDeviation > 0.0f))
			{
				FRawMesh InMesh = LODMeshes[ReductionSettings.BaseLODModel];
				FRawMesh& DestMesh = LODMeshes[NumValidLODs];
				TMultiMap<int32, int32>& DestOverlappingCorners = LODOverlappingCorners[NumValidLODs];

				MeshReduction->Reduce(DestMesh, LODMaxDeviation[NumValidLODs], InMesh, ReductionSettings);
				if (DestMesh.WedgeIndices.Num() > 0 && !DestMesh.IsValid())
				{
					UE_LOG(LogMeshUtilities, Error, TEXT("Mesh reduction produced a corrupt mesh for LOD%d"), LODIndex);
					return false;
				}
				bOutWasReduced = true;

				// Recompute adjacency information.
				DestOverlappingCorners.Reset();
				float ComparisonThreshold = GetComparisonThreshold(LODBuildSettings[NumValidLODs]);
				FindOverlappingCorners(DestOverlappingCorners, DestMesh, ComparisonThreshold);
			}

			if (LODMeshes[NumValidLODs].WedgeIndices.Num() > 0)
			{
				NumValidLODs++;
			}
		}

		if (NumValidLODs < 1)
		{
			return false;
		}
		Stage = EStage::Reduce;
		return true;
	}

	bool GenerateRenderingMeshes(FMeshUtilities& MeshUtilities, FStaticMeshRenderData& OutRenderData, TArray<FStaticMeshSourceModel>& InOutModels)
	{
		check(Stage == EStage::Reduce);
		// Generate per-LOD rendering data.
		OutRenderData.AllocateLODResources(NumValidLODs);
		for (int32 LODIndex = 0; LODIndex < NumValidLODs; ++LODIndex)
		{
			FStaticMeshLODResources& LODModel = OutRenderData.LODResources[LODIndex];
			FRawMesh& RawMesh = LODMeshes[LODIndex];
			LODModel.MaxDeviation = LODMaxDeviation[LODIndex];

			TArray<FStaticMeshBuildVertex> Vertices;
			TArray<TArray<uint32> > PerSectionIndices;

			// Find out how many sections are in the mesh.
			int32 MaxMaterialIndex = -1;
			for (int32 FaceIndex = 0; FaceIndex < RawMesh.FaceMaterialIndices.Num(); FaceIndex++)
			{
				MaxMaterialIndex = FMath::Max<int32>(RawMesh.FaceMaterialIndices[FaceIndex], MaxMaterialIndex);
			}

			while (MaxMaterialIndex >= LODModel.Sections.Num())
			{
				FStaticMeshSection* Section = new(LODModel.Sections) FStaticMeshSection();
				Section->MaterialIndex = LODModel.Sections.Num() - 1;
				new(PerSectionIndices)TArray<uint32>;
			}

			// Build and cache optimize vertex and index buffers.
			{
				// TODO_STATICMESH: The wedge map is only valid for LODIndex 0 if no reduction has been performed.
				// We can compute an approximate one instead for other LODs.
				TArray<int32> TempWedgeMap;
				TArray<int32>& WedgeMap = (LODIndex == 0 && InOutModels[0].ReductionSettings.PercentTriangles >= 1.0f) ? OutRenderData.WedgeMap : TempWedgeMap;
				float ComparisonThreshold = GetComparisonThreshold(LODBuildSettings[LODIndex]);
				BuildStaticMeshVertexAndIndexBuffers(Vertices, PerSectionIndices, WedgeMap, RawMesh, LODOverlappingCorners[LODIndex], ComparisonThreshold, LODBuildSettings[LODIndex].BuildScale3D);
				check(WedgeMap.Num() == RawMesh.WedgeIndices.Num());

				if (RawMesh.WedgeIndices.Num() < 100000 * 3)
				{
					MeshUtilities.CacheOptimizeVertexAndIndexBuffer(Vertices, PerSectionIndices, WedgeMap);
					check(WedgeMap.Num() == RawMesh.WedgeIndices.Num());
				}
			}

			verifyf(Vertices.Num() != 0, TEXT("No valid vertices found for the mesh."));

			// Initialize the vertex buffer.
			int32 NumTexCoords = ComputeNumTexCoords(RawMesh, MAX_STATIC_TEXCOORDS);
			LODModel.VertexBuffer.SetUseHighPrecisionTangentBasis(LODBuildSettings[LODIndex].bUseHighPrecisionTangentBasis);
			LODModel.VertexBuffer.SetUseFullPrecisionUVs(LODBuildSettings[LODIndex].bUseFullPrecisionUVs);
			LODModel.VertexBuffer.Init(Vertices, NumTexCoords);
			LODModel.PositionVertexBuffer.Init(Vertices);
			LODModel.ColorVertexBuffer.Init(Vertices);

			// Concatenate the per-section index buffers.
			TArray<uint32> CombinedIndices;
			bool bNeeds32BitIndices = false;
			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
			{
				FStaticMeshSection& Section = LODModel.Sections[SectionIndex];
				TArray<uint32> const& SectionIndices = PerSectionIndices[SectionIndex];
				Section.FirstIndex = 0;
				Section.NumTriangles = 0;
				Section.MinVertexIndex = 0;
				Section.MaxVertexIndex = 0;

				if (SectionIndices.Num())
				{
					Section.FirstIndex = CombinedIndices.Num();
					Section.NumTriangles = SectionIndices.Num() / 3;

					CombinedIndices.AddUninitialized(SectionIndices.Num());
					uint32* DestPtr = &CombinedIndices[Section.FirstIndex];
					uint32 const* SrcPtr = SectionIndices.GetData();

					Section.MinVertexIndex = *SrcPtr;
					Section.MaxVertexIndex = *SrcPtr;

					for (int32 Index = 0; Index < SectionIndices.Num(); Index++)
					{
						uint32 VertIndex = *SrcPtr++;

						bNeeds32BitIndices |= (VertIndex > MAX_uint16);
						Section.MinVertexIndex = FMath::Min<uint32>(VertIndex, Section.MinVertexIndex);
						Section.MaxVertexIndex = FMath::Max<uint32>(VertIndex, Section.MaxVertexIndex);
						*DestPtr++ = VertIndex;
					}
				}
			}
			LODModel.IndexBuffer.SetIndices(CombinedIndices, bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit);

			if (LODIndex == 0)
			{
				ComputeStreamingTextureFactors(
					OutRenderData.StreamingTextureFactors,
					&OutRenderData.MaxStreamingTextureFactor,
					RawMesh,
					LODBuildSettings[LODIndex].BuildScale3D
					);
			}

			// Build the reversed index buffer.
			if (InOutModels[0].BuildSettings.bBuildReversedIndexBuffer)
			{
				TArray<uint32> InversedIndices;
				const int32 IndexCount = CombinedIndices.Num();
				InversedIndices.AddUninitialized(IndexCount);

				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
				{
					const FStaticMeshSection& SectionInfo = LODModel.Sections[SectionIndex];
					const int32 SectionIndexCount = SectionInfo.NumTriangles * 3;

					for (int32 i = 0; i < SectionIndexCount; ++i)
					{
						InversedIndices[SectionInfo.FirstIndex + i] = CombinedIndices[SectionInfo.FirstIndex + SectionIndexCount - 1 - i];
					}
				}
				LODModel.ReversedIndexBuffer.SetIndices(InversedIndices, bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit);
			}

			// Build the depth-only index buffer.
			TArray<uint32> DepthOnlyIndices;
			{
				BuildDepthOnlyIndexBuffer(
					DepthOnlyIndices,
					Vertices,
					CombinedIndices,
					LODModel.Sections
					);

				if (DepthOnlyIndices.Num() < 50000 * 3)
				{
					MeshUtilities.CacheOptimizeIndexBuffer(DepthOnlyIndices);
				}

				LODModel.DepthOnlyIndexBuffer.SetIndices(DepthOnlyIndices, bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit);
			}

			// Build the inversed depth only index buffer.
			if (InOutModels[0].BuildSettings.bBuildReversedIndexBuffer)
			{
				TArray<uint32> ReversedDepthOnlyIndices;
				const int32 IndexCount = DepthOnlyIndices.Num();
				ReversedDepthOnlyIndices.AddUninitialized(IndexCount);
				for (int32 i = 0; i < IndexCount; ++i)
				{
					ReversedDepthOnlyIndices[i] = DepthOnlyIndices[IndexCount - 1 - i];
				}
				LODModel.ReversedDepthOnlyIndexBuffer.SetIndices(ReversedDepthOnlyIndices, bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit);
			}

			// Build a list of wireframe edges in the static mesh.
			{
				TArray<FMeshEdge> Edges;
				TArray<uint32> WireframeIndices;

				FStaticMeshEdgeBuilder(CombinedIndices, Vertices, Edges).FindEdges();
				WireframeIndices.Empty(2 * Edges.Num());
				for (int32 EdgeIndex = 0; EdgeIndex < Edges.Num(); EdgeIndex++)
				{
					FMeshEdge&	Edge = Edges[EdgeIndex];
					WireframeIndices.Add(Edge.Vertices[0]);
					WireframeIndices.Add(Edge.Vertices[1]);
				}
				LODModel.WireframeIndexBuffer.SetIndices(WireframeIndices, bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit);
			}

			// Build the adjacency index buffer used for tessellation.
			if (InOutModels[0].BuildSettings.bBuildAdjacencyBuffer)
			{
				TArray<uint32> AdjacencyIndices;

				BuildStaticAdjacencyIndexBuffer(
					LODModel.PositionVertexBuffer,
					LODModel.VertexBuffer,
					CombinedIndices,
					AdjacencyIndices
					);
				LODModel.AdjacencyIndexBuffer.SetIndices(AdjacencyIndices, bNeeds32BitIndices ? EIndexBufferStride::Force32Bit : EIndexBufferStride::Force16Bit);
			}
		}

		// Copy the original material indices to fixup meshes before compacting of materials was done.
		if (NumValidLODs > 0)
		{
			OutRenderData.MaterialIndexToImportIndex = LODMeshes[0].MaterialIndexToImportIndex;
		}

		// Calculate the bounding box.
		FBox BoundingBox(0);
		FPositionVertexBuffer& BasePositionVertexBuffer = OutRenderData.LODResources[0].PositionVertexBuffer;
		for (uint32 VertexIndex = 0; VertexIndex < BasePositionVertexBuffer.GetNumVertices(); VertexIndex++)
		{
			BoundingBox += BasePositionVertexBuffer.VertexPosition(VertexIndex);
		}
		BoundingBox.GetCenterAndExtents(OutRenderData.Bounds.Origin, OutRenderData.Bounds.BoxExtent);

		// Calculate the bounding sphere, using the center of the bounding box as the origin.
		OutRenderData.Bounds.SphereRadius = 0.0f;
		for (uint32 VertexIndex = 0; VertexIndex < BasePositionVertexBuffer.GetNumVertices(); VertexIndex++)
		{
			OutRenderData.Bounds.SphereRadius = FMath::Max(
				(BasePositionVertexBuffer.VertexPosition(VertexIndex) - OutRenderData.Bounds.Origin).Size(),
				OutRenderData.Bounds.SphereRadius
				);
		}

		Stage = EStage::GenerateRendering;
		return true;
	}

	bool ReplaceRawMeshModels(TArray<FStaticMeshSourceModel>& SourceModels)
	{
		check(Stage == EStage::Reduce);

		check(HasRawMesh[0]);
		check(SourceModels.Num() >= NumValidLODs);
		bool bDirty = false;
		for (int32 Index = 1; Index < NumValidLODs; ++Index)
		{
			if (!HasRawMesh[Index])
			{
				SourceModels[Index].RawMeshBulkData->SaveRawMesh(LODMeshes[Index]);
				bDirty = true;
			}
		}

		Stage = EStage::ReplaceRaw;
		return true;
	}

private:
	enum class EStage
	{
		Uninit,
		Gathered,
		Reduce,
		GenerateRendering,
		ReplaceRaw,
	};

	EStage Stage;

	int32 NumValidLODs;

	TIndirectArray<FRawMesh> LODMeshes;
	TIndirectArray<TMultiMap<int32, int32> > LODOverlappingCorners;
	float LODMaxDeviation[MAX_STATIC_MESH_LODS];
	FMeshBuildSettings LODBuildSettings[MAX_STATIC_MESH_LODS];
	bool HasRawMesh[MAX_STATIC_MESH_LODS];
};

bool FMeshUtilities::BuildStaticMesh(FStaticMeshRenderData& OutRenderData, TArray<FStaticMeshSourceModel>& SourceModels, const FStaticMeshLODGroup& LODGroup)
{
	FStaticMeshUtilityBuilder Builder;
	if (!Builder.GatherSourceMeshesPerLOD(SourceModels, MeshReduction))
	{
		return false;
	}

	OutRenderData.bReducedBySimplygon = false;
	bool bWasReduced = false;
	if (!Builder.ReduceLODs(SourceModels, LODGroup, MeshReduction, bWasReduced))
	{
		return false;
	}
	OutRenderData.bReducedBySimplygon = (bWasReduced && bUsingSimplygon);

	return Builder.GenerateRenderingMeshes(*this, OutRenderData, SourceModels);
}

bool FMeshUtilities::GenerateStaticMeshLODs(TArray<FStaticMeshSourceModel>& Models, const FStaticMeshLODGroup& LODGroup)
{
	FStaticMeshUtilityBuilder Builder;
	if (!Builder.GatherSourceMeshesPerLOD(Models, MeshReduction))
	{
		return false;
	}

	bool bWasReduced = false;
	if (!Builder.ReduceLODs(Models, LODGroup, MeshReduction, bWasReduced))
	{
		return false;
	}

	if (bWasReduced)
	{
		return Builder.ReplaceRawMeshModels(Models);
	}

	return false;
}

class IMeshBuildData
{
public:
	virtual uint32 GetWedgeIndex(uint32 FaceIndex, uint32 TriIndex) = 0;
	virtual uint32 GetVertexIndex(uint32 WedgeIndex) = 0;
	virtual uint32 GetVertexIndex(uint32 FaceIndex, uint32 TriIndex) = 0;
	virtual FVector GetVertexPosition(uint32 WedgeIndex) = 0;
	virtual FVector GetVertexPosition(uint32 FaceIndex, uint32 TriIndex) = 0;
	virtual FVector2D GetVertexUV(uint32 FaceIndex, uint32 TriIndex, uint32 UVIndex) = 0;
	virtual uint32 GetFaceSmoothingGroups(uint32 FaceIndex) = 0;

	virtual uint32 GetNumFaces() = 0;
	virtual uint32 GetNumWedges() = 0;

	virtual TArray<FVector>& GetTangentArray(uint32 Axis) = 0;
	virtual void ValidateTangentArraySize() = 0;

	virtual SMikkTSpaceInterface* GetMikkTInterface() = 0;
	virtual void* GetMikkTUserData() = 0;

	const IMeshUtilities::MeshBuildOptions& BuildOptions;
	TArray<FText>* OutWarningMessages;
	TArray<FName>* OutWarningNames;
	bool bTooManyVerts;

protected:
	IMeshBuildData(
		const IMeshUtilities::MeshBuildOptions& InBuildOptions,
		TArray<FText>* InWarningMessages,
		TArray<FName>* InWarningNames)
		: BuildOptions(InBuildOptions)
		, OutWarningMessages(InWarningMessages)
		, OutWarningNames(InWarningNames)
		, bTooManyVerts(false)
	{
	}
};

class SkeletalMeshBuildData : public IMeshBuildData
{
public:
	SkeletalMeshBuildData(
		FStaticLODModel& InLODModel,
		const FReferenceSkeleton& InRefSkeleton,
		const TArray<FVertInfluence>& InInfluences,
		const TArray<FMeshWedge>& InWedges,
		const TArray<FMeshFace>& InFaces,
		const TArray<FVector>& InPoints,
		const TArray<int32>& InPointToOriginalMap,
		const IMeshUtilities::MeshBuildOptions& InBuildOptions,
		TArray<FText>* InWarningMessages,
		TArray<FName>* InWarningNames)
		: IMeshBuildData(InBuildOptions, InWarningMessages, InWarningNames)
		, MikkTUserData(InWedges, InFaces, InPoints, InBuildOptions.bComputeNormals, TangentX, TangentY, TangentZ)
		, LODModel(InLODModel)
		, RefSkeleton(InRefSkeleton)
		, Influences(InInfluences)
		, Wedges(InWedges)
		, Faces(InFaces)
		, Points(InPoints)
		, PointToOriginalMap(InPointToOriginalMap)
	{
		MikkTInterface.m_getNormal = MikkGetNormal_Skeletal;
		MikkTInterface.m_getNumFaces = MikkGetNumFaces_Skeletal;
		MikkTInterface.m_getNumVerticesOfFace = MikkGetNumVertsOfFace_Skeletal;
		MikkTInterface.m_getPosition = MikkGetPosition_Skeletal;
		MikkTInterface.m_getTexCoord = MikkGetTexCoord_Skeletal;
		MikkTInterface.m_setTSpaceBasic = MikkSetTSpaceBasic_Skeletal;
		MikkTInterface.m_setTSpace = nullptr;
	}

	virtual uint32 GetWedgeIndex(uint32 FaceIndex, uint32 TriIndex) override
	{
		return Faces[FaceIndex].iWedge[TriIndex];
	}

	virtual uint32 GetVertexIndex(uint32 WedgeIndex) override
	{
		return Wedges[WedgeIndex].iVertex;
	}

	virtual uint32 GetVertexIndex(uint32 FaceIndex, uint32 TriIndex) override
	{
		return Wedges[Faces[FaceIndex].iWedge[TriIndex]].iVertex;
	}

	virtual FVector GetVertexPosition(uint32 WedgeIndex) override
	{
		return Points[Wedges[WedgeIndex].iVertex];
	}

	virtual FVector GetVertexPosition(uint32 FaceIndex, uint32 TriIndex) override
	{
		return Points[Wedges[Faces[FaceIndex].iWedge[TriIndex]].iVertex];
	}

	virtual FVector2D GetVertexUV(uint32 FaceIndex, uint32 TriIndex, uint32 UVIndex) override
	{
		return Wedges[Faces[FaceIndex].iWedge[TriIndex]].UVs[UVIndex];
	}

	virtual uint32 GetFaceSmoothingGroups(uint32 FaceIndex)
	{
		return Faces[FaceIndex].SmoothingGroups;
	}

	virtual uint32 GetNumFaces() override
	{
		return Faces.Num();
	}

	virtual uint32 GetNumWedges() override
	{
		return Wedges.Num();
	}

	virtual TArray<FVector>& GetTangentArray(uint32 Axis) override
	{
		if (Axis == 0)
		{
			return TangentX;
		}
		else if (Axis == 1)
		{
			return TangentY;
		}

		return TangentZ;
	}

	virtual void ValidateTangentArraySize() override
	{
		check(TangentX.Num() == Wedges.Num());
		check(TangentY.Num() == Wedges.Num());
		check(TangentZ.Num() == Wedges.Num());
	}

	virtual SMikkTSpaceInterface* GetMikkTInterface() override
	{
		return &MikkTInterface;
	}

	virtual void* GetMikkTUserData() override
	{
		return (void*)&MikkTUserData;
	}

	TArray<FVector> TangentX;
	TArray<FVector> TangentY;
	TArray<FVector> TangentZ;
	TArray<FSkinnedMeshChunk*> Chunks;

	SMikkTSpaceInterface MikkTInterface;
	MikkTSpace_Skeletal_Mesh MikkTUserData;

	FStaticLODModel& LODModel;
	const FReferenceSkeleton& RefSkeleton;
	const TArray<FVertInfluence>& Influences;
	const TArray<FMeshWedge>& Wedges;
	const TArray<FMeshFace>& Faces;
	const TArray<FVector>& Points;
	const TArray<int32>& PointToOriginalMap;
};

class FSkeletalMeshUtilityBuilder
{
public:
	FSkeletalMeshUtilityBuilder()
		: Stage(EStage::Uninit)
	{
	}

public:
	void Skeletal_FindOverlappingCorners(
		TMultiMap<int32, int32>& OutOverlappingCorners,
		IMeshBuildData* BuildData,
		float ComparisonThreshold
		)
	{
		int32 NumFaces = BuildData->GetNumFaces();
		int32 NumWedges = BuildData->GetNumWedges();
		check(NumFaces * 3 <= NumWedges);

		// Create a list of vertex Z/index pairs
		TArray<FIndexAndZ> VertIndexAndZ;
		VertIndexAndZ.Empty(NumWedges);
		for (int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
		{
			for (int32 TriIndex = 0; TriIndex < 3; ++TriIndex)
			{
				uint32 Index = BuildData->GetWedgeIndex(FaceIndex, TriIndex);
				new(VertIndexAndZ)FIndexAndZ(Index, BuildData->GetVertexPosition(Index));
			}
		}

		// Sort the vertices by z value
		VertIndexAndZ.Sort(FCompareIndexAndZ());

		// Search for duplicates, quickly!
		for (int32 i = 0; i < VertIndexAndZ.Num(); i++)
		{
			// only need to search forward, since we add pairs both ways
			for (int32 j = i + 1; j < VertIndexAndZ.Num(); j++)
			{
				if (FMath::Abs(VertIndexAndZ[j].Z - VertIndexAndZ[i].Z) > ComparisonThreshold)
					break; // can't be any more dups

				FVector PositionA = BuildData->GetVertexPosition(VertIndexAndZ[i].Index);
				FVector PositionB = BuildData->GetVertexPosition(VertIndexAndZ[j].Index);

				if (PointsEqual(PositionA, PositionB, ComparisonThreshold))
				{
					OutOverlappingCorners.Add(VertIndexAndZ[i].Index, VertIndexAndZ[j].Index);
					OutOverlappingCorners.Add(VertIndexAndZ[j].Index, VertIndexAndZ[i].Index);
				}
			}
		}
	}

	void Skeletal_ComputeTriangleTangents(
		TArray<FVector>& TriangleTangentX,
		TArray<FVector>& TriangleTangentY,
		TArray<FVector>& TriangleTangentZ,
		IMeshBuildData* BuildData,
		float ComparisonThreshold
		)
	{
		int32 NumTriangles = BuildData->GetNumFaces();
		TriangleTangentX.Empty(NumTriangles);
		TriangleTangentY.Empty(NumTriangles);
		TriangleTangentZ.Empty(NumTriangles);

		for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++)
		{
			const int32 UVIndex = 0;
			FVector P[3];

			for (int32 i = 0; i < 3; ++i)
			{
				P[i] = BuildData->GetVertexPosition(TriangleIndex, i);
			}

			const FVector Normal = ((P[1] - P[2]) ^ (P[0] - P[2])).GetSafeNormal(ComparisonThreshold);
			FMatrix	ParameterToLocal(
				FPlane(P[1].X - P[0].X, P[1].Y - P[0].Y, P[1].Z - P[0].Z, 0),
				FPlane(P[2].X - P[0].X, P[2].Y - P[0].Y, P[2].Z - P[0].Z, 0),
				FPlane(P[0].X, P[0].Y, P[0].Z, 0),
				FPlane(0, 0, 0, 1)
				);

			FVector2D T1 = BuildData->GetVertexUV(TriangleIndex, 0, UVIndex);
			FVector2D T2 = BuildData->GetVertexUV(TriangleIndex, 1, UVIndex);
			FVector2D T3 = BuildData->GetVertexUV(TriangleIndex, 2, UVIndex);
			FMatrix ParameterToTexture(
				FPlane(T2.X - T1.X, T2.Y - T1.Y, 0, 0),
				FPlane(T3.X - T1.X, T3.Y - T1.Y, 0, 0),
				FPlane(T1.X, T1.Y, 1, 0),
				FPlane(0, 0, 0, 1)
				);

			// Use InverseSlow to catch singular matrices.  Inverse can miss this sometimes.
			const FMatrix TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;

			TriangleTangentX.Add(TextureToLocal.TransformVector(FVector(1, 0, 0)).GetSafeNormal());
			TriangleTangentY.Add(TextureToLocal.TransformVector(FVector(0, 1, 0)).GetSafeNormal());
			TriangleTangentZ.Add(Normal);

			FVector::CreateOrthonormalBasis(
				TriangleTangentX[TriangleIndex],
				TriangleTangentY[TriangleIndex],
				TriangleTangentZ[TriangleIndex]
				);
		}
	}

	void Skeletal_ComputeTangents(
		IMeshBuildData* BuildData,
		TMultiMap<int32, int32> const& OverlappingCorners
		)
	{
		bool bBlendOverlappingNormals = true;
		bool bIgnoreDegenerateTriangles = BuildData->BuildOptions.bRemoveDegenerateTriangles;
		float ComparisonThreshold = bIgnoreDegenerateTriangles ? THRESH_POINTS_ARE_SAME : 0.0f;

		// Compute per-triangle tangents.
		TArray<FVector> TriangleTangentX;
		TArray<FVector> TriangleTangentY;
		TArray<FVector> TriangleTangentZ;

		Skeletal_ComputeTriangleTangents(
			TriangleTangentX,
			TriangleTangentY,
			TriangleTangentZ,
			BuildData,
			bIgnoreDegenerateTriangles ? SMALL_NUMBER : 0.0f
			);

		TArray<FVector>& WedgeTangentX = BuildData->GetTangentArray(0);
		TArray<FVector>& WedgeTangentY = BuildData->GetTangentArray(1);
		TArray<FVector>& WedgeTangentZ = BuildData->GetTangentArray(2);

		// Declare these out here to avoid reallocations.
		TArray<FFanFace> RelevantFacesForCorner[3];
		TArray<int32> AdjacentFaces;
		TArray<int32> DupVerts;

		int32 NumFaces = BuildData->GetNumFaces();
		int32 NumWedges = BuildData->GetNumWedges();
		check(NumFaces * 3 <= NumWedges);

		// Allocate storage for tangents if none were provided.
		if (WedgeTangentX.Num() != NumWedges)
		{
			WedgeTangentX.Empty(NumWedges);
			WedgeTangentX.AddZeroed(NumWedges);
		}
		if (WedgeTangentY.Num() != NumWedges)
		{
			WedgeTangentY.Empty(NumWedges);
			WedgeTangentY.AddZeroed(NumWedges);
		}
		if (WedgeTangentZ.Num() != NumWedges)
		{
			WedgeTangentZ.Empty(NumWedges);
			WedgeTangentZ.AddZeroed(NumWedges);
		}

		for (int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
		{
			int32 WedgeOffset = FaceIndex * 3;
			FVector CornerPositions[3];
			FVector CornerTangentX[3];
			FVector CornerTangentY[3];
			FVector CornerTangentZ[3];

			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				CornerTangentX[CornerIndex] = FVector::ZeroVector;
				CornerTangentY[CornerIndex] = FVector::ZeroVector;
				CornerTangentZ[CornerIndex] = FVector::ZeroVector;
				CornerPositions[CornerIndex] = BuildData->GetVertexPosition(FaceIndex, CornerIndex);
				RelevantFacesForCorner[CornerIndex].Reset();
			}

			// Don't process degenerate triangles.
			if (PointsEqual(CornerPositions[0], CornerPositions[1], ComparisonThreshold)
				|| PointsEqual(CornerPositions[0], CornerPositions[2], ComparisonThreshold)
				|| PointsEqual(CornerPositions[1], CornerPositions[2], ComparisonThreshold))
			{
				continue;
			}

			// No need to process triangles if tangents already exist.
			bool bCornerHasTangents[3] = { 0 };
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				bCornerHasTangents[CornerIndex] = !WedgeTangentX[WedgeOffset + CornerIndex].IsZero()
					&& !WedgeTangentY[WedgeOffset + CornerIndex].IsZero()
					&& !WedgeTangentZ[WedgeOffset + CornerIndex].IsZero();
			}
			if (bCornerHasTangents[0] && bCornerHasTangents[1] && bCornerHasTangents[2])
			{
				continue;
			}

			// Calculate smooth vertex normals.
			float Determinant = FVector::Triple(
				TriangleTangentX[FaceIndex],
				TriangleTangentY[FaceIndex],
				TriangleTangentZ[FaceIndex]
				);

			// Start building a list of faces adjacent to this face.
			AdjacentFaces.Reset();
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				int32 ThisCornerIndex = WedgeOffset + CornerIndex;
				DupVerts.Reset();
				OverlappingCorners.MultiFind(ThisCornerIndex, DupVerts);
				DupVerts.Add(ThisCornerIndex); // I am a "dup" of myself
				for (int32 k = 0; k < DupVerts.Num(); k++)
				{
					AdjacentFaces.AddUnique(DupVerts[k] / 3);
				}
			}

			// We need to sort these here because the criteria for point equality is
			// exact, so we must ensure the exact same order for all dups.
			AdjacentFaces.Sort();

			// Process adjacent faces
			for (int32 AdjacentFaceIndex = 0; AdjacentFaceIndex < AdjacentFaces.Num(); AdjacentFaceIndex++)
			{
				int32 OtherFaceIndex = AdjacentFaces[AdjacentFaceIndex];
				for (int32 OurCornerIndex = 0; OurCornerIndex < 3; OurCornerIndex++)
				{
					if (bCornerHasTangents[OurCornerIndex])
						continue;

					FFanFace NewFanFace;
					int32 CommonIndexCount = 0;

					// Check for vertices in common.
					if (FaceIndex == OtherFaceIndex)
					{
						CommonIndexCount = 3;
						NewFanFace.LinkedVertexIndex = OurCornerIndex;
					}
					else
					{
						// Check matching vertices against main vertex .
						for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
						{
							if (PointsEqual(
								CornerPositions[OurCornerIndex],
								BuildData->GetVertexPosition(OtherFaceIndex, OtherCornerIndex),
								ComparisonThreshold
								))
							{
								CommonIndexCount++;
								NewFanFace.LinkedVertexIndex = OtherCornerIndex;
							}
						}
					}

					// Add if connected by at least one point. Smoothing matches are considered later.
					if (CommonIndexCount > 0)
					{
						NewFanFace.FaceIndex = OtherFaceIndex;
						NewFanFace.bFilled = (OtherFaceIndex == FaceIndex); // Starter face for smoothing floodfill.
						NewFanFace.bBlendTangents = NewFanFace.bFilled;
						NewFanFace.bBlendNormals = NewFanFace.bFilled;
						RelevantFacesForCorner[OurCornerIndex].Add(NewFanFace);
					}
				}
			}

			// Find true relevance of faces for a vertex normal by traversing
			// smoothing-group-compatible connected triangle fans around common vertices.
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				if (bCornerHasTangents[CornerIndex])
					continue;

				int32 NewConnections;
				do
				{
					NewConnections = 0;
					for (int32 OtherFaceIdx = 0; OtherFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); OtherFaceIdx++)
					{
						FFanFace& OtherFace = RelevantFacesForCorner[CornerIndex][OtherFaceIdx];
						// The vertex' own face is initially the only face with bFilled == true.
						if (OtherFace.bFilled)
						{
							for (int32 NextFaceIndex = 0; NextFaceIndex < RelevantFacesForCorner[CornerIndex].Num(); NextFaceIndex++)
							{
								FFanFace& NextFace = RelevantFacesForCorner[CornerIndex][NextFaceIndex];
								if (!NextFace.bFilled) // && !NextFace.bBlendTangents)
								{
									if (NextFaceIndex != OtherFaceIdx)
										//&& (RawMesh.FaceSmoothingMasks[NextFace.FaceIndex] & RawMesh.FaceSmoothingMasks[OtherFace.FaceIndex]))
									{
										int32 CommonVertices = 0;
										int32 CommonTangentVertices = 0;
										int32 CommonNormalVertices = 0;
										for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
										{
											for (int32 NextCornerIndex = 0; NextCornerIndex < 3; NextCornerIndex++)
											{
												int32 NextVertexIndex = BuildData->GetVertexIndex(NextFace.FaceIndex, NextCornerIndex);
												int32 OtherVertexIndex = BuildData->GetVertexIndex(OtherFace.FaceIndex, OtherCornerIndex);
												if (PointsEqual(
													BuildData->GetVertexPosition(NextFace.FaceIndex, NextCornerIndex),
													BuildData->GetVertexPosition(OtherFace.FaceIndex, OtherCornerIndex),
													ComparisonThreshold))
												{
													CommonVertices++;


													if (UVsEqual(
														BuildData->GetVertexUV(NextFace.FaceIndex, NextCornerIndex, 0),
														BuildData->GetVertexUV(OtherFace.FaceIndex, OtherCornerIndex, 0)))
													{
														CommonTangentVertices++;
													}
													if (bBlendOverlappingNormals
														|| NextVertexIndex == OtherVertexIndex)
													{
														CommonNormalVertices++;
													}
												}
											}
										}
										// Flood fill faces with more than one common vertices which must be touching edges.
										if (CommonVertices > 1)
										{
											NextFace.bFilled = true;
											NextFace.bBlendNormals = (CommonNormalVertices > 1);
											NewConnections++;

											// Only blend tangents if there is no UV seam along the edge with this face.
											if (OtherFace.bBlendTangents && CommonTangentVertices > 1)
											{
												float OtherDeterminant = FVector::Triple(
													TriangleTangentX[NextFace.FaceIndex],
													TriangleTangentY[NextFace.FaceIndex],
													TriangleTangentZ[NextFace.FaceIndex]
													);
												if ((Determinant * OtherDeterminant) > 0.0f)
												{
													NextFace.bBlendTangents = true;
												}
											}
										}
									}
								}
							}
						}
					}
				}
				while (NewConnections > 0);
			}

			// Vertex normal construction.
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				if (bCornerHasTangents[CornerIndex])
				{
					CornerTangentX[CornerIndex] = WedgeTangentX[WedgeOffset + CornerIndex];
					CornerTangentY[CornerIndex] = WedgeTangentY[WedgeOffset + CornerIndex];
					CornerTangentZ[CornerIndex] = WedgeTangentZ[WedgeOffset + CornerIndex];
				}
				else
				{
					for (int32 RelevantFaceIdx = 0; RelevantFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); RelevantFaceIdx++)
					{
						FFanFace const& RelevantFace = RelevantFacesForCorner[CornerIndex][RelevantFaceIdx];
						if (RelevantFace.bFilled)
						{
							int32 OtherFaceIndex = RelevantFace.FaceIndex;
							if (RelevantFace.bBlendTangents)
							{
								CornerTangentX[CornerIndex] += TriangleTangentX[OtherFaceIndex];
								CornerTangentY[CornerIndex] += TriangleTangentY[OtherFaceIndex];
							}
							if (RelevantFace.bBlendNormals)
							{
								CornerTangentZ[CornerIndex] += TriangleTangentZ[OtherFaceIndex];
							}
						}
					}
					if (!WedgeTangentX[WedgeOffset + CornerIndex].IsZero())
					{
						CornerTangentX[CornerIndex] = WedgeTangentX[WedgeOffset + CornerIndex];
					}
					if (!WedgeTangentY[WedgeOffset + CornerIndex].IsZero())
					{
						CornerTangentY[CornerIndex] = WedgeTangentY[WedgeOffset + CornerIndex];
					}
					if (!WedgeTangentZ[WedgeOffset + CornerIndex].IsZero())
					{
						CornerTangentZ[CornerIndex] = WedgeTangentZ[WedgeOffset + CornerIndex];
					}
				}
			}

			// Normalization.
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				CornerTangentX[CornerIndex].Normalize();
				CornerTangentY[CornerIndex].Normalize();
				CornerTangentZ[CornerIndex].Normalize();

				// Gram-Schmidt orthogonalization
				CornerTangentY[CornerIndex] -= CornerTangentX[CornerIndex] * (CornerTangentX[CornerIndex] | CornerTangentY[CornerIndex]);
				CornerTangentY[CornerIndex].Normalize();

				CornerTangentX[CornerIndex] -= CornerTangentZ[CornerIndex] * (CornerTangentZ[CornerIndex] | CornerTangentX[CornerIndex]);
				CornerTangentX[CornerIndex].Normalize();
				CornerTangentY[CornerIndex] -= CornerTangentZ[CornerIndex] * (CornerTangentZ[CornerIndex] | CornerTangentY[CornerIndex]);
				CornerTangentY[CornerIndex].Normalize();
			}

			// Copy back to the mesh.
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
			{
				WedgeTangentX[WedgeOffset + CornerIndex] = CornerTangentX[CornerIndex];
				WedgeTangentY[WedgeOffset + CornerIndex] = CornerTangentY[CornerIndex];
				WedgeTangentZ[WedgeOffset + CornerIndex] = CornerTangentZ[CornerIndex];
			}
		}

		check(WedgeTangentX.Num() == NumWedges);
		check(WedgeTangentY.Num() == NumWedges);
		check(WedgeTangentZ.Num() == NumWedges);
	}

	void Skeletal_ComputeTangents_MikkTSpace(
		IMeshBuildData* BuildData,
		TMultiMap<int32, int32> const& OverlappingCorners
		)
	{
		bool bBlendOverlappingNormals = true;
		bool bIgnoreDegenerateTriangles = BuildData->BuildOptions.bRemoveDegenerateTriangles;
		float ComparisonThreshold = bIgnoreDegenerateTriangles ? THRESH_POINTS_ARE_SAME : 0.0f;

		// Compute per-triangle tangents.
		TArray<FVector> TriangleTangentX;
		TArray<FVector> TriangleTangentY;
		TArray<FVector> TriangleTangentZ;

		Skeletal_ComputeTriangleTangents(
			TriangleTangentX,
			TriangleTangentY,
			TriangleTangentZ,
			BuildData,
			bIgnoreDegenerateTriangles ? SMALL_NUMBER : 0.0f
			);

		TArray<FVector>& WedgeTangentX = BuildData->GetTangentArray(0);
		TArray<FVector>& WedgeTangentY = BuildData->GetTangentArray(1);
		TArray<FVector>& WedgeTangentZ = BuildData->GetTangentArray(2);

		// Declare these out here to avoid reallocations.
		TArray<FFanFace> RelevantFacesForCorner[3];
		TArray<int32> AdjacentFaces;
		TArray<int32> DupVerts;

		int32 NumFaces = BuildData->GetNumFaces();
		int32 NumWedges = BuildData->GetNumWedges();
		check(NumFaces * 3 == NumWedges);

		bool bWedgeNormals = true;
		bool bWedgeTSpace = false;
		for (int32 WedgeIdx = 0; WedgeIdx < WedgeTangentZ.Num(); ++WedgeIdx)
		{
			for (int32 VertexIndex = 0; VertexIndex < 3; VertexIndex++)
				bWedgeNormals = bWedgeNormals && (!WedgeTangentZ[WedgeIdx].IsNearlyZero());
		}

		if (WedgeTangentX.Num() > 0 && WedgeTangentY.Num() > 0)
		{
			bWedgeTSpace = true;
			for (int32 WedgeIdx = 0; WedgeIdx < WedgeTangentX.Num()
				&& WedgeIdx < WedgeTangentY.Num(); ++WedgeIdx)
			{
				bWedgeTSpace = bWedgeTSpace && (!WedgeTangentX[WedgeIdx].IsNearlyZero()) && (!WedgeTangentY[WedgeIdx].IsNearlyZero());
			}
		}

		// Allocate storage for tangents if none were provided, and calculate normals for MikkTSpace.
		if (WedgeTangentZ.Num() != NumWedges || !bWedgeNormals)
		{
			// normals are not included, so we should calculate them
			WedgeTangentZ.Empty(NumWedges);
			WedgeTangentZ.AddZeroed(NumWedges);
			// we need to calculate normals for MikkTSpace

			for (int32 FaceIndex = 0; FaceIndex < NumFaces; FaceIndex++)
			{
				int32 WedgeOffset = FaceIndex * 3;
				FVector CornerPositions[3];
				FVector CornerNormal[3];

				for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
				{
					CornerNormal[CornerIndex] = FVector::ZeroVector;
					CornerPositions[CornerIndex] = BuildData->GetVertexPosition(FaceIndex, CornerIndex);
					RelevantFacesForCorner[CornerIndex].Reset();
				}

				// Don't process degenerate triangles.
				if (PointsEqual(CornerPositions[0], CornerPositions[1], ComparisonThreshold)
					|| PointsEqual(CornerPositions[0], CornerPositions[2], ComparisonThreshold)
					|| PointsEqual(CornerPositions[1], CornerPositions[2], ComparisonThreshold))
				{
					continue;
				}

				// No need to process triangles if tangents already exist.
				bool bCornerHasNormal[3] = { 0 };
				for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
				{
					bCornerHasNormal[CornerIndex] = !WedgeTangentZ[WedgeOffset + CornerIndex].IsZero();
				}
				if (bCornerHasNormal[0] && bCornerHasNormal[1] && bCornerHasNormal[2])
				{
					continue;
				}

				// Start building a list of faces adjacent to this face.
				AdjacentFaces.Reset();
				for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
				{
					int32 ThisCornerIndex = WedgeOffset + CornerIndex;
					DupVerts.Reset();
					OverlappingCorners.MultiFind(ThisCornerIndex, DupVerts);
					DupVerts.Add(ThisCornerIndex); // I am a "dup" of myself
					for (int32 k = 0; k < DupVerts.Num(); k++)
					{
						AdjacentFaces.AddUnique(DupVerts[k] / 3);
					}
				}

				// We need to sort these here because the criteria for point equality is
				// exact, so we must ensure the exact same order for all dups.
				AdjacentFaces.Sort();

				// Process adjacent faces
				for (int32 AdjacentFaceIndex = 0; AdjacentFaceIndex < AdjacentFaces.Num(); AdjacentFaceIndex++)
				{
					int32 OtherFaceIndex = AdjacentFaces[AdjacentFaceIndex];
					for (int32 OurCornerIndex = 0; OurCornerIndex < 3; OurCornerIndex++)
					{
						if (bCornerHasNormal[OurCornerIndex])
							continue;

						FFanFace NewFanFace;
						int32 CommonIndexCount = 0;

						// Check for vertices in common.
						if (FaceIndex == OtherFaceIndex)
						{
							CommonIndexCount = 3;
							NewFanFace.LinkedVertexIndex = OurCornerIndex;
						}
						else
						{
							// Check matching vertices against main vertex .
							for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
							{
								if (PointsEqual(
									CornerPositions[OurCornerIndex],
									BuildData->GetVertexPosition(OtherFaceIndex, OtherCornerIndex),
									ComparisonThreshold
									))
								{
									CommonIndexCount++;
									NewFanFace.LinkedVertexIndex = OtherCornerIndex;
								}
							}
						}

						// Add if connected by at least one point. Smoothing matches are considered later.
						if (CommonIndexCount > 0)
						{
							NewFanFace.FaceIndex = OtherFaceIndex;
							NewFanFace.bFilled = (OtherFaceIndex == FaceIndex); // Starter face for smoothing floodfill.
							NewFanFace.bBlendTangents = NewFanFace.bFilled;
							NewFanFace.bBlendNormals = NewFanFace.bFilled;
							RelevantFacesForCorner[OurCornerIndex].Add(NewFanFace);
						}
					}
				}

				// Find true relevance of faces for a vertex normal by traversing
				// smoothing-group-compatible connected triangle fans around common vertices.
				for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
				{
					if (bCornerHasNormal[CornerIndex])
						continue;

					int32 NewConnections;
					do
					{
						NewConnections = 0;
						for (int32 OtherFaceIdx = 0; OtherFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); OtherFaceIdx++)
						{
							FFanFace& OtherFace = RelevantFacesForCorner[CornerIndex][OtherFaceIdx];
							// The vertex' own face is initially the only face with bFilled == true.
							if (OtherFace.bFilled)
							{
								for (int32 NextFaceIndex = 0; NextFaceIndex < RelevantFacesForCorner[CornerIndex].Num(); NextFaceIndex++)
								{
									FFanFace& NextFace = RelevantFacesForCorner[CornerIndex][NextFaceIndex];
									if (!NextFace.bFilled) // && !NextFace.bBlendTangents)
									{
										if ((NextFaceIndex != OtherFaceIdx)
											 && (BuildData->GetFaceSmoothingGroups(NextFace.FaceIndex) & BuildData->GetFaceSmoothingGroups(OtherFace.FaceIndex)))
										{
											int32 CommonVertices = 0;
											int32 CommonNormalVertices = 0;
											for (int32 OtherCornerIndex = 0; OtherCornerIndex < 3; OtherCornerIndex++)
											{
												for (int32 NextCornerIndex = 0; NextCornerIndex < 3; NextCornerIndex++)
												{
													int32 NextVertexIndex = BuildData->GetVertexIndex(NextFace.FaceIndex, NextCornerIndex);
													int32 OtherVertexIndex = BuildData->GetVertexIndex(OtherFace.FaceIndex, OtherCornerIndex);
													if (PointsEqual(
														BuildData->GetVertexPosition(NextFace.FaceIndex, NextCornerIndex),
														BuildData->GetVertexPosition(OtherFace.FaceIndex, OtherCornerIndex),
														ComparisonThreshold))
													{
														CommonVertices++;
														if (bBlendOverlappingNormals
															|| NextVertexIndex == OtherVertexIndex)
														{
															CommonNormalVertices++;
														}
													}
												}
											}
											// Flood fill faces with more than one common vertices which must be touching edges.
											if (CommonVertices > 1)
											{
												NextFace.bFilled = true;
												NextFace.bBlendNormals = (CommonNormalVertices > 1);
												NewConnections++;
											}
										}
									}
								}
							}
						}
					} 
					while (NewConnections > 0);
				}

				// Vertex normal construction.
				for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
				{
					if (bCornerHasNormal[CornerIndex])
					{
						CornerNormal[CornerIndex] = WedgeTangentZ[WedgeOffset + CornerIndex];
					}
					else
					{
						for (int32 RelevantFaceIdx = 0; RelevantFaceIdx < RelevantFacesForCorner[CornerIndex].Num(); RelevantFaceIdx++)
						{
							FFanFace const& RelevantFace = RelevantFacesForCorner[CornerIndex][RelevantFaceIdx];
							if (RelevantFace.bFilled)
							{
								int32 OtherFaceIndex = RelevantFace.FaceIndex;
								if (RelevantFace.bBlendNormals)
								{
									CornerNormal[CornerIndex] += TriangleTangentZ[OtherFaceIndex];
								}
							}
						}
						if (!WedgeTangentZ[WedgeOffset + CornerIndex].IsZero())
						{
							CornerNormal[CornerIndex] = WedgeTangentZ[WedgeOffset + CornerIndex];
						}
					}
				}

				// Normalization.
				for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
				{
					CornerNormal[CornerIndex].Normalize();
				}

				// Copy back to the mesh.
				for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++)
				{
					WedgeTangentZ[WedgeOffset + CornerIndex] = CornerNormal[CornerIndex];
				}
			}
		}

		if (WedgeTangentX.Num() != NumWedges)
		{
			WedgeTangentX.Empty(NumWedges);
			WedgeTangentX.AddZeroed(NumWedges);
		}
		if (WedgeTangentY.Num() != NumWedges)
		{
			WedgeTangentY.Empty(NumWedges);
			WedgeTangentY.AddZeroed(NumWedges);
		}

		//if (!bWedgeTSpace)
		{
			// we can use mikktspace to calculate the tangents		
			SMikkTSpaceContext MikkTContext;
			MikkTContext.m_pInterface = BuildData->GetMikkTInterface();
			MikkTContext.m_pUserData = BuildData->GetMikkTUserData();
			//MikkTContext.m_bIgnoreDegenerates = bIgnoreDegenerateTriangles;

			genTangSpaceDefault(&MikkTContext);
		}

		check(WedgeTangentX.Num() == NumWedges);
		check(WedgeTangentY.Num() == NumWedges);
		check(WedgeTangentZ.Num() == NumWedges);
	}

	bool PrepareSourceMesh(IMeshBuildData* BuildData)
	{
		check(Stage == EStage::Uninit);

		BeginSlowTask();

		TMultiMap<int32, int32>& OverlappingCorners = *new(LODOverlappingCorners)TMultiMap<int32, int32>;

		float ComparisonThreshold = THRESH_POINTS_ARE_SAME;//GetComparisonThreshold(LODBuildSettings[LODIndex]);
		int32 NumWedges = BuildData->GetNumWedges();

		// Find overlapping corners to accelerate adjacency.
		Skeletal_FindOverlappingCorners(OverlappingCorners, BuildData, ComparisonThreshold);

		// Figure out if we should recompute normals and tangents.
		bool bRecomputeNormals = BuildData->BuildOptions.bComputeNormals;
		bool bRecomputeTangents = BuildData->BuildOptions.bComputeTangents;

		// Dump normals and tangents if we are recomputing them.
		if (bRecomputeTangents)
		{
			TArray<FVector>& TangentX = BuildData->GetTangentArray(0);
			TArray<FVector>& TangentY = BuildData->GetTangentArray(1);

			TangentX.Empty(NumWedges);
			TangentX.AddZeroed(NumWedges);
			TangentY.Empty(NumWedges);
			TangentY.AddZeroed(NumWedges);
		}
		if (bRecomputeNormals)
		{
			TArray<FVector>& TangentZ = BuildData->GetTangentArray(2);
			TangentZ.Empty(NumWedges);
			TangentZ.AddZeroed(NumWedges);
		}

		// Compute any missing tangents. MikkTSpace should be use only when the user want to recompute the normals or tangents otherwise should always fallback on builtin
		if (BuildData->BuildOptions.bUseMikkTSpace && (BuildData->BuildOptions.bComputeNormals || BuildData->BuildOptions.bComputeTangents))
		{
			Skeletal_ComputeTangents_MikkTSpace(BuildData, OverlappingCorners);
		}
		else
		{
			Skeletal_ComputeTangents(BuildData, OverlappingCorners);
		}

		// At this point the mesh will have valid tangents.
		BuildData->ValidateTangentArraySize();
		check(LODOverlappingCorners.Num() == 1);

		EndSlowTask();

		Stage = EStage::Prepared;
		return true;
	}

	bool GenerateSkeletalRenderMesh(IMeshBuildData* InBuildData)
	{
		check(Stage == EStage::Prepared);

		SkeletalMeshBuildData& BuildData = *(SkeletalMeshBuildData*)InBuildData;

		BeginSlowTask();

		// Find wedge influences.
		TArray<int32>	WedgeInfluenceIndices;
		TMap<uint32, uint32> VertexIndexToInfluenceIndexMap;

		for (uint32 LookIdx = 0; LookIdx < (uint32)BuildData.Influences.Num(); LookIdx++)
		{
			// Order matters do not allow the map to overwrite an existing value.
			if (!VertexIndexToInfluenceIndexMap.Find(BuildData.Influences[LookIdx].VertIndex))
			{
				VertexIndexToInfluenceIndexMap.Add(BuildData.Influences[LookIdx].VertIndex, LookIdx);
			}
		}

		for (int32 WedgeIndex = 0; WedgeIndex < BuildData.Wedges.Num(); WedgeIndex++)
		{
			uint32* InfluenceIndex = VertexIndexToInfluenceIndexMap.Find(BuildData.Wedges[WedgeIndex].iVertex);

			if (InfluenceIndex)
			{
				WedgeInfluenceIndices.Add(*InfluenceIndex);
			}
			else
			{
				// we have missing influence vert,  we weight to root
				WedgeInfluenceIndices.Add(0);

				// add warning message
				if (BuildData.OutWarningMessages)
				{
					BuildData.OutWarningMessages->Add(FText::Format(FText::FromString("Missing influence on vert {0}. Weighting it to root."), FText::FromString(FString::FromInt(BuildData.Wedges[WedgeIndex].iVertex))));
					if (BuildData.OutWarningNames)
					{
						BuildData.OutWarningNames->Add(FFbxErrors::SkeletalMesh_VertMissingInfluences);
					}
				}
			}
		}

		check(BuildData.Wedges.Num() == WedgeInfluenceIndices.Num());

		TArray<FSkeletalMeshVertIndexAndZ> VertIndexAndZ;
		TArray<FSoftSkinBuildVertex> RawVertices;

		VertIndexAndZ.Empty(BuildData.Points.Num());
		RawVertices.Reserve(BuildData.Points.Num());

		for (int32 FaceIndex = 0; FaceIndex < BuildData.Faces.Num(); FaceIndex++)
		{
			// Only update the status progress bar if we are in the game thread and every thousand faces. 
			// Updating status is extremely slow
			if (FaceIndex % 5000 == 0)
			{
				UpdateSlowTask(FaceIndex, BuildData.Faces.Num());
			}

			const FMeshFace& Face = BuildData.Faces[FaceIndex];

			for (int32 VertexIndex = 0; VertexIndex < 3; VertexIndex++)
			{
				FSoftSkinBuildVertex Vertex;
				const uint32 WedgeIndex = BuildData.GetWedgeIndex(FaceIndex, VertexIndex);
				const FMeshWedge& Wedge = BuildData.Wedges[WedgeIndex];

				Vertex.Position = BuildData.GetVertexPosition(FaceIndex, VertexIndex);

				FVector TangentX, TangentY, TangentZ;
				TangentX = BuildData.TangentX[WedgeIndex].GetSafeNormal();
				TangentY = BuildData.TangentY[WedgeIndex].GetSafeNormal();
				TangentZ = BuildData.TangentZ[WedgeIndex].GetSafeNormal();

				/*if (BuildData.BuildOptions.bComputeNormals || BuildData.BuildOptions.bComputeTangents)
				{
				TangentX = BuildData.TangentX[VertexIndex].GetSafeNormal();
				TangentY = BuildData.TangentY[VertexIndex].GetSafeNormal();

				if( BuildData.BuildOptions.bComputeNormals )
				{
				TangentZ = BuildData.TangentZ[VertexIndex].GetSafeNormal();
				}
				else
				{
				//TangentZ = Face.TangentZ[VertexIndex];
				}

				TangentY -= TangentX * (TangentX | TangentY);
				TangentY.Normalize();

				TangentX -= TangentZ * (TangentZ | TangentX);
				TangentY -= TangentZ * (TangentZ | TangentY);

				TangentX.Normalize();
				TangentY.Normalize();
				}
				else*/
				{
					//TangentX = Face.TangentX[VertexIndex];
					//TangentY = Face.TangentY[VertexIndex];
					//TangentZ = Face.TangentZ[VertexIndex];

					// Normalize overridden tangents.  Its possible for them to import un-normalized.
					TangentX.Normalize();
					TangentY.Normalize();
					TangentZ.Normalize();
				}

				Vertex.TangentX = TangentX;
				Vertex.TangentY = TangentY;
				Vertex.TangentZ = TangentZ;

				FMemory::Memcpy(Vertex.UVs, Wedge.UVs, sizeof(FVector2D)*MAX_TEXCOORDS);
				Vertex.Color = Wedge.Color;

				{
					// Count the influences.
					int32 InfIdx = WedgeInfluenceIndices[Face.iWedge[VertexIndex]];
					int32 LookIdx = InfIdx;

					uint32 InfluenceCount = 0;
					while (BuildData.Influences.IsValidIndex(LookIdx) && (BuildData.Influences[LookIdx].VertIndex == Wedge.iVertex))
					{
						InfluenceCount++;
						LookIdx++;
					}
					InfluenceCount = FMath::Min<uint32>(InfluenceCount, MAX_TOTAL_INFLUENCES);

					// Setup the vertex influences.
					Vertex.InfluenceBones[0] = 0;
					Vertex.InfluenceWeights[0] = 255;
					for (uint32 i = 1; i < MAX_TOTAL_INFLUENCES; i++)
					{
						Vertex.InfluenceBones[i] = 0;
						Vertex.InfluenceWeights[i] = 0;
					}

					uint32	TotalInfluenceWeight = 0;
					for (uint32 i = 0; i < InfluenceCount; i++)
					{
						FBoneIndexType BoneIndex = (FBoneIndexType)BuildData.Influences[InfIdx + i].BoneIndex;
						if (BoneIndex >= BuildData.RefSkeleton.GetNum())
							continue;

						Vertex.InfluenceBones[i] = BoneIndex;
						Vertex.InfluenceWeights[i] = (uint8)(BuildData.Influences[InfIdx + i].Weight * 255.0f);
						TotalInfluenceWeight += Vertex.InfluenceWeights[i];
					}
					Vertex.InfluenceWeights[0] += 255 - TotalInfluenceWeight;
				}

				// Add the vertex as well as its original index in the points array
				Vertex.PointWedgeIdx = Wedge.iVertex;

				int32 RawIndex = RawVertices.Add(Vertex);

				// Add an efficient way to find dupes of this vertex later for fast combining of vertices
				FSkeletalMeshVertIndexAndZ IAndZ;
				IAndZ.Index = RawIndex;
				IAndZ.Z = Vertex.Position.Z;

				VertIndexAndZ.Add(IAndZ);
			}
		}

		// Generate chunks and their vertices and indices
		SkeletalMeshTools::BuildSkeletalMeshChunks(BuildData.Faces, RawVertices, VertIndexAndZ, BuildData.BuildOptions.bKeepOverlappingVertices, BuildData.Chunks, BuildData.bTooManyVerts);

		// Chunk vertices to satisfy the requested limit.
		const uint32 MaxGPUSkinBones = FGPUBaseSkinVertexFactory::GetMaxGPUSkinBones();
		check(MaxGPUSkinBones <= FGPUBaseSkinVertexFactory::GHardwareMaxGPUSkinBones);
		SkeletalMeshTools::ChunkSkinnedVertices(BuildData.Chunks, MaxGPUSkinBones);

		EndSlowTask();

		Stage = EStage::GenerateRendering;
		return true;
	}

	void BeginSlowTask()
	{
		if (IsInGameThread())
		{
			GWarn->BeginSlowTask(NSLOCTEXT("UnrealEd", "ProcessingSkeletalTriangles", "Processing Mesh Triangles"), true);
		}
	}

	void UpdateSlowTask(int32 Numerator, int32 Denominator)
	{
		if (IsInGameThread())
		{
			GWarn->StatusUpdate(Numerator, Denominator, NSLOCTEXT("UnrealEd", "ProcessingSkeletalTriangles", "Processing Mesh Triangles"));
		}
	}

	void EndSlowTask()
	{
		if (IsInGameThread())
		{
			GWarn->EndSlowTask();
		}
	}

private:
	enum class EStage
	{
		Uninit,
		Prepared,
		GenerateRendering,
	};

	TIndirectArray<TMultiMap<int32, int32> > LODOverlappingCorners;
	EStage Stage;
};

bool FMeshUtilities::BuildSkeletalMesh(FStaticLODModel& LODModel, const FReferenceSkeleton& RefSkeleton, const TArray<FVertInfluence>& Influences, const TArray<FMeshWedge>& Wedges, const TArray<FMeshFace>& Faces, const TArray<FVector>& Points, const TArray<int32>& PointToOriginalMap, const MeshBuildOptions& BuildOptions, TArray<FText> * OutWarningMessages, TArray<FName> * OutWarningNames)
{
#if WITH_EDITORONLY_DATA
	// Temporarily supporting both import paths
	if (!BuildOptions.bUseMikkTSpace)
	{
		return BuildSkeletalMesh_Legacy(LODModel, RefSkeleton, Influences, Wedges, Faces, Points, PointToOriginalMap, BuildOptions.bKeepOverlappingVertices, BuildOptions.bComputeNormals, BuildOptions.bComputeTangents, OutWarningMessages, OutWarningNames);
	}

	SkeletalMeshBuildData BuildData(
		LODModel,
		RefSkeleton,
		Influences,
		Wedges,
		Faces,
		Points,
		PointToOriginalMap,
		BuildOptions,
		OutWarningMessages,
		OutWarningNames);

	FSkeletalMeshUtilityBuilder Builder;
	if (!Builder.PrepareSourceMesh(&BuildData))
	{
		return false;
	}

	if (!Builder.GenerateSkeletalRenderMesh(&BuildData))
	{
		return false;
	}

	// Build the skeletal model from chunks.
	Builder.BeginSlowTask();
	BuildSkeletalModelFromChunks(BuildData.LODModel, BuildData.RefSkeleton, BuildData.Chunks, BuildData.PointToOriginalMap);
	Builder.EndSlowTask();

	// Only show these warnings if in the game thread.  When importing morph targets, this function can run in another thread and these warnings dont prevent the mesh from importing
	if (IsInGameThread())
	{
		bool bHasBadSections = false;
		for (int32 SectionIndex = 0; SectionIndex < BuildData.LODModel.Sections.Num(); SectionIndex++)
		{
			FSkelMeshSection& Section = BuildData.LODModel.Sections[SectionIndex];
			bHasBadSections |= (Section.NumTriangles == 0);

			// Log info about the section.
			UE_LOG(LogSkeletalMesh, Log, TEXT("Section %u: Material=%u, %u triangles"),
				SectionIndex,
				Section.MaterialIndex,
				Section.NumTriangles
				);
		}
		if (bHasBadSections)
		{
			FText BadSectionMessage(NSLOCTEXT("UnrealEd", "Error_SkeletalMeshHasBadSections", "Input mesh has a section with no triangles.  This mesh may not render properly."));
			if (BuildData.OutWarningMessages)
			{
				BuildData.OutWarningMessages->Add(BadSectionMessage);
				if (BuildData.OutWarningNames)
				{
					BuildData.OutWarningNames->Add(FFbxErrors::SkeletalMesh_SectionWithNoTriangle);
				}
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, BadSectionMessage);
			}
		}

		if (BuildData.bTooManyVerts)
		{
			FText TooManyVertsMessage(NSLOCTEXT("UnrealEd", "Error_SkeletalMeshTooManyVertices", "Input mesh has too many vertices.  The generated mesh will be corrupt!  Consider adding extra materials to split up the source mesh into smaller chunks."));

			if (BuildData.OutWarningMessages)
			{
				BuildData.OutWarningMessages->Add(TooManyVertsMessage);
				if (BuildData.OutWarningNames)
				{
					BuildData.OutWarningNames->Add(FFbxErrors::SkeletalMesh_TooManyVertices);
				}
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, TooManyVertsMessage);
			}
		}
	}

	return true;
#else
	if (OutWarningMessages)
	{
		OutWarningMessages->Add(FText::FromString(TEXT("Cannot call FMeshUtilities::BuildSkeletalMesh on a console!")));
	}
	else
	{
		UE_LOG(LogSkeletalMesh, Fatal, TEXT("Cannot call FMeshUtilities::BuildSkeletalMesh on a console!"));
	}
	return false;
#endif
}

//@TODO: The OutMessages has to be a struct that contains FText/FName, or make it Token and add that as error. Needs re-work. Temporary workaround for now. 
bool FMeshUtilities::BuildSkeletalMesh_Legacy(FStaticLODModel& LODModel, const FReferenceSkeleton& RefSkeleton, const TArray<FVertInfluence>& Influences, const TArray<FMeshWedge>& Wedges, const TArray<FMeshFace>& Faces, const TArray<FVector>& Points, const TArray<int32>& PointToOriginalMap, bool bKeepOverlappingVertices, bool bComputeNormals, bool bComputeTangents, TArray<FText> * OutWarningMessages, TArray<FName> * OutWarningNames)
{
	bool bTooManyVerts = false;

	check(PointToOriginalMap.Num() == Points.Num());

	// Calculate face tangent vectors.
	TArray<FVector>	FaceTangentX;
	TArray<FVector>	FaceTangentY;
	FaceTangentX.AddUninitialized(Faces.Num());
	FaceTangentY.AddUninitialized(Faces.Num());

	if (bComputeNormals || bComputeTangents)
	{
		for (int32 FaceIndex = 0; FaceIndex < Faces.Num(); FaceIndex++)
		{
			FVector	P1 = Points[Wedges[Faces[FaceIndex].iWedge[0]].iVertex],
				P2 = Points[Wedges[Faces[FaceIndex].iWedge[1]].iVertex],
				P3 = Points[Wedges[Faces[FaceIndex].iWedge[2]].iVertex];
			FVector	TriangleNormal = FPlane(P3, P2, P1);
			FMatrix	ParameterToLocal(
				FPlane(P2.X - P1.X, P2.Y - P1.Y, P2.Z - P1.Z, 0),
				FPlane(P3.X - P1.X, P3.Y - P1.Y, P3.Z - P1.Z, 0),
				FPlane(P1.X, P1.Y, P1.Z, 0),
				FPlane(0, 0, 0, 1)
				);

			float	U1 = Wedges[Faces[FaceIndex].iWedge[0]].UVs[0].X,
				U2 = Wedges[Faces[FaceIndex].iWedge[1]].UVs[0].X,
				U3 = Wedges[Faces[FaceIndex].iWedge[2]].UVs[0].X,
				V1 = Wedges[Faces[FaceIndex].iWedge[0]].UVs[0].Y,
				V2 = Wedges[Faces[FaceIndex].iWedge[1]].UVs[0].Y,
				V3 = Wedges[Faces[FaceIndex].iWedge[2]].UVs[0].Y;

			FMatrix	ParameterToTexture(
				FPlane(U2 - U1, V2 - V1, 0, 0),
				FPlane(U3 - U1, V3 - V1, 0, 0),
				FPlane(U1, V1, 1, 0),
				FPlane(0, 0, 0, 1)
				);

			FMatrix	TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;
			FVector	TangentX = TextureToLocal.TransformVector(FVector(1, 0, 0)).GetSafeNormal(),
				TangentY = TextureToLocal.TransformVector(FVector(0, 1, 0)).GetSafeNormal(),
				TangentZ;

			TangentX = TangentX - TriangleNormal * (TangentX | TriangleNormal);
			TangentY = TangentY - TriangleNormal * (TangentY | TriangleNormal);

			FaceTangentX[FaceIndex] = TangentX.GetSafeNormal();
			FaceTangentY[FaceIndex] = TangentY.GetSafeNormal();
		}
	}

	TArray<int32>	WedgeInfluenceIndices;

	// Find wedge influences.
	TMap<uint32, uint32> VertexIndexToInfluenceIndexMap;

	for (uint32 LookIdx = 0; LookIdx < (uint32)Influences.Num(); LookIdx++)
	{
		// Order matters do not allow the map to overwrite an existing value.
		if (!VertexIndexToInfluenceIndexMap.Find(Influences[LookIdx].VertIndex))
		{
			VertexIndexToInfluenceIndexMap.Add(Influences[LookIdx].VertIndex, LookIdx);
		}
	}

	for (int32 WedgeIndex = 0; WedgeIndex < Wedges.Num(); WedgeIndex++)
	{
		uint32* InfluenceIndex = VertexIndexToInfluenceIndexMap.Find(Wedges[WedgeIndex].iVertex);

		if (InfluenceIndex)
		{
			WedgeInfluenceIndices.Add(*InfluenceIndex);
		}
		else
		{
			// we have missing influence vert,  we weight to root
			WedgeInfluenceIndices.Add(0);

			// add warning message
			if (OutWarningMessages)
			{
				OutWarningMessages->Add(FText::Format(FText::FromString("Missing influence on vert {0}. Weighting it to root."), FText::FromString(FString::FromInt(Wedges[WedgeIndex].iVertex))));
				if (OutWarningNames)
				{
					OutWarningNames->Add(FFbxErrors::SkeletalMesh_VertMissingInfluences);
				}
			}
		}
	}

	check(Wedges.Num() == WedgeInfluenceIndices.Num());

	// Calculate smooth wedge tangent vectors.

	if (IsInGameThread())
	{
		// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
		GWarn->BeginSlowTask(NSLOCTEXT("UnrealEd", "ProcessingSkeletalTriangles", "Processing Mesh Triangles"), true);
	}


	// To accelerate generation of adjacency, we'll create a table that maps each vertex index
	// to its overlapping vertices, and a table that maps a vertex to the its influenced faces
	TMultiMap<int32, int32> Vert2Duplicates;
	TMultiMap<int32, int32> Vert2Faces;
	TArray<FSkeletalMeshVertIndexAndZ> VertIndexAndZ;
	{
		// Create a list of vertex Z/index pairs
		VertIndexAndZ.Empty(Points.Num());
		for (int32 i = 0; i < Points.Num(); i++)
		{
			FSkeletalMeshVertIndexAndZ iandz;
			iandz.Index = i;
			iandz.Z = Points[i].Z;
			VertIndexAndZ.Add(iandz);
		}

		// Sorting function for vertex Z/index pairs
		struct FCompareFSkeletalMeshVertIndexAndZ
		{
			FORCEINLINE bool operator()(const FSkeletalMeshVertIndexAndZ& A, const FSkeletalMeshVertIndexAndZ& B) const
			{
				return A.Z < B.Z;
			}
		};

		// Sort the vertices by z value
		VertIndexAndZ.Sort(FCompareFSkeletalMeshVertIndexAndZ());

		// Search for duplicates, quickly!
		for (int32 i = 0; i < VertIndexAndZ.Num(); i++)
		{
			// only need to search forward, since we add pairs both ways
			for (int32 j = i + 1; j < VertIndexAndZ.Num(); j++)
			{
				if (FMath::Abs(VertIndexAndZ[j].Z - VertIndexAndZ[i].Z) > THRESH_POINTS_ARE_SAME)
				{
					// our list is sorted, so there can't be any more dupes
					break;
				}

				// check to see if the points are really overlapping
				if (PointsEqual(
					Points[VertIndexAndZ[i].Index],
					Points[VertIndexAndZ[j].Index]))
				{
					Vert2Duplicates.Add(VertIndexAndZ[i].Index, VertIndexAndZ[j].Index);
					Vert2Duplicates.Add(VertIndexAndZ[j].Index, VertIndexAndZ[i].Index);
				}
			}
		}

		// we are done with this
		VertIndexAndZ.Reset();

		// now create a map from vert indices to faces
		for (int32 FaceIndex = 0; FaceIndex < Faces.Num(); FaceIndex++)
		{
			const FMeshFace&	Face = Faces[FaceIndex];
			for (int32 VertexIndex = 0; VertexIndex < 3; VertexIndex++)
			{
				Vert2Faces.AddUnique(Wedges[Face.iWedge[VertexIndex]].iVertex, FaceIndex);
			}
		}
	}

	TArray<FSkinnedMeshChunk*> Chunks;
	TArray<int32> AdjacentFaces;
	TArray<int32> DupVerts;
	TArray<int32> DupFaces;

	// List of raw calculated vertices that will be merged later
	TArray<FSoftSkinBuildVertex> RawVertices;
	RawVertices.Reserve(Points.Num());

	// Create a list of vertex Z/index pairs

	for (int32 FaceIndex = 0; FaceIndex < Faces.Num(); FaceIndex++)
	{
		// Only update the status progress bar if we are in the gamethread and every thousand faces. 
		// Updating status is extremely slow
		if (FaceIndex % 5000 == 0 && IsInGameThread())
		{
			// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
			GWarn->StatusUpdate(FaceIndex, Faces.Num(), NSLOCTEXT("UnrealEd", "ProcessingSkeletalTriangles", "Processing Mesh Triangles"));
		}

		const FMeshFace&	Face = Faces[FaceIndex];

		FVector	VertexTangentX[3],
			VertexTangentY[3],
			VertexTangentZ[3];

		if (bComputeNormals || bComputeTangents)
		{
			for (int32 VertexIndex = 0; VertexIndex < 3; VertexIndex++)
			{
				VertexTangentX[VertexIndex] = FVector::ZeroVector;
				VertexTangentY[VertexIndex] = FVector::ZeroVector;
				VertexTangentZ[VertexIndex] = FVector::ZeroVector;
			}

			FVector	TriangleNormal = FPlane(
				Points[Wedges[Face.iWedge[2]].iVertex],
				Points[Wedges[Face.iWedge[1]].iVertex],
				Points[Wedges[Face.iWedge[0]].iVertex]
				);
			float	Determinant = FVector::Triple(FaceTangentX[FaceIndex], FaceTangentY[FaceIndex], TriangleNormal);

			// Start building a list of faces adjacent to this triangle
			AdjacentFaces.Reset();
			for (int32 VertexIndex = 0; VertexIndex < 3; VertexIndex++)
			{
				int32 vert = Wedges[Face.iWedge[VertexIndex]].iVertex;
				DupVerts.Reset();
				Vert2Duplicates.MultiFind(vert, DupVerts);
				DupVerts.Add(vert); // I am a "dupe" of myself
				for (int32 k = 0; k < DupVerts.Num(); k++)
				{
					DupFaces.Reset();
					Vert2Faces.MultiFind(DupVerts[k], DupFaces);
					for (int32 l = 0; l < DupFaces.Num(); l++)
					{
						AdjacentFaces.AddUnique(DupFaces[l]);
					}
				}
			}

			// Process adjacent faces
			for (int32 AdjacentFaceIndex = 0; AdjacentFaceIndex < AdjacentFaces.Num(); AdjacentFaceIndex++)
			{
				int32 OtherFaceIndex = AdjacentFaces[AdjacentFaceIndex];
				const FMeshFace&	OtherFace = Faces[OtherFaceIndex];
				FVector		OtherTriangleNormal = FPlane(
					Points[Wedges[OtherFace.iWedge[2]].iVertex],
					Points[Wedges[OtherFace.iWedge[1]].iVertex],
					Points[Wedges[OtherFace.iWedge[0]].iVertex]
					);
				float		OtherFaceDeterminant = FVector::Triple(FaceTangentX[OtherFaceIndex], FaceTangentY[OtherFaceIndex], OtherTriangleNormal);

				for (int32 VertexIndex = 0; VertexIndex < 3; VertexIndex++)
				{
					for (int32 OtherVertexIndex = 0; OtherVertexIndex < 3; OtherVertexIndex++)
					{
						if (PointsEqual(
							Points[Wedges[OtherFace.iWedge[OtherVertexIndex]].iVertex],
							Points[Wedges[Face.iWedge[VertexIndex]].iVertex]
							))
						{
							if (Determinant * OtherFaceDeterminant > 0.0f && SkeletalMeshTools::SkeletalMesh_UVsEqual(Wedges[OtherFace.iWedge[OtherVertexIndex]], Wedges[Face.iWedge[VertexIndex]]))
							{
								VertexTangentX[VertexIndex] += FaceTangentX[OtherFaceIndex];
								VertexTangentY[VertexIndex] += FaceTangentY[OtherFaceIndex];
							}

							// Only contribute 'normal' if the vertices are truly one and the same to obey hard "smoothing" edges baked into 
							// the mesh by vertex duplication
							if (Wedges[OtherFace.iWedge[OtherVertexIndex]].iVertex == Wedges[Face.iWedge[VertexIndex]].iVertex)
							{
								VertexTangentZ[VertexIndex] += OtherTriangleNormal;
							}
						}
					}
				}
			}
		}

		for (int32 VertexIndex = 0; VertexIndex < 3; VertexIndex++)
		{
			FSoftSkinBuildVertex	Vertex;

			Vertex.Position = Points[Wedges[Face.iWedge[VertexIndex]].iVertex];

			FVector TangentX, TangentY, TangentZ;

			if (bComputeNormals || bComputeTangents)
			{
				TangentX = VertexTangentX[VertexIndex].GetSafeNormal();
				TangentY = VertexTangentY[VertexIndex].GetSafeNormal();

				if (bComputeNormals)
				{
					TangentZ = VertexTangentZ[VertexIndex].GetSafeNormal();
				}
				else
				{
					TangentZ = Face.TangentZ[VertexIndex];
				}

				TangentY -= TangentX * (TangentX | TangentY);
				TangentY.Normalize();

				TangentX -= TangentZ * (TangentZ | TangentX);
				TangentY -= TangentZ * (TangentZ | TangentY);

				TangentX.Normalize();
				TangentY.Normalize();
			}
			else
			{
				TangentX = Face.TangentX[VertexIndex];
				TangentY = Face.TangentY[VertexIndex];
				TangentZ = Face.TangentZ[VertexIndex];

				// Normalize overridden tangents.  Its possible for them to import un-normalized.
				TangentX.Normalize();
				TangentY.Normalize();
				TangentZ.Normalize();
			}

			Vertex.TangentX = TangentX;
			Vertex.TangentY = TangentY;
			Vertex.TangentZ = TangentZ;

			FMemory::Memcpy(Vertex.UVs, Wedges[Face.iWedge[VertexIndex]].UVs, sizeof(FVector2D)*MAX_TEXCOORDS);
			Vertex.Color = Wedges[Face.iWedge[VertexIndex]].Color;

			{
				// Count the influences.

				int32 InfIdx = WedgeInfluenceIndices[Face.iWedge[VertexIndex]];
				int32 LookIdx = InfIdx;

				uint32 InfluenceCount = 0;
				while (Influences.IsValidIndex(LookIdx) && (Influences[LookIdx].VertIndex == Wedges[Face.iWedge[VertexIndex]].iVertex))
				{
					InfluenceCount++;
					LookIdx++;
				}
				InfluenceCount = FMath::Min<uint32>(InfluenceCount, MAX_TOTAL_INFLUENCES);

				// Setup the vertex influences.

				Vertex.InfluenceBones[0] = 0;
				Vertex.InfluenceWeights[0] = 255;
				for (uint32 i = 1; i < MAX_TOTAL_INFLUENCES; i++)
				{
					Vertex.InfluenceBones[i] = 0;
					Vertex.InfluenceWeights[i] = 0;
				}

				uint32	TotalInfluenceWeight = 0;
				for (uint32 i = 0; i < InfluenceCount; i++)
				{
					FBoneIndexType BoneIndex = (FBoneIndexType)Influences[InfIdx + i].BoneIndex;
					if (BoneIndex >= RefSkeleton.GetNum())
						continue;

					Vertex.InfluenceBones[i] = BoneIndex;
					Vertex.InfluenceWeights[i] = (uint8)(Influences[InfIdx + i].Weight * 255.0f);
					TotalInfluenceWeight += Vertex.InfluenceWeights[i];
				}
				Vertex.InfluenceWeights[0] += 255 - TotalInfluenceWeight;
			}

			// Add the vertex as well as its original index in the points array
			Vertex.PointWedgeIdx = Wedges[Face.iWedge[VertexIndex]].iVertex;

			int32 RawIndex = RawVertices.Add(Vertex);

			// Add an efficient way to find dupes of this vertex later for fast combining of vertices
			FSkeletalMeshVertIndexAndZ IAndZ;
			IAndZ.Index = RawIndex;
			IAndZ.Z = Vertex.Position.Z;

			VertIndexAndZ.Add(IAndZ);
		}
	}

	// Generate chunks and their vertices and indices
	SkeletalMeshTools::BuildSkeletalMeshChunks(Faces, RawVertices, VertIndexAndZ, bKeepOverlappingVertices, Chunks, bTooManyVerts);

	// Chunk vertices to satisfy the requested limit.
	const uint32 MaxGPUSkinBones = FGPUBaseSkinVertexFactory::GetMaxGPUSkinBones();
	check(MaxGPUSkinBones <= FGPUBaseSkinVertexFactory::GHardwareMaxGPUSkinBones);
	SkeletalMeshTools::ChunkSkinnedVertices(Chunks, MaxGPUSkinBones);

	// Build the skeletal model from chunks.
	BuildSkeletalModelFromChunks(LODModel, RefSkeleton, Chunks, PointToOriginalMap);

	if (IsInGameThread())
	{
		// Only update status if in the game thread.  When importing morph targets, this function can run in another thread
		GWarn->EndSlowTask();
	}

	// Only show these warnings if in the game thread.  When importing morph targets, this function can run in another thread and these warnings dont prevent the mesh from importing
	if (IsInGameThread())
	{
		bool bHasBadSections = false;
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
		{
			FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
			bHasBadSections |= (Section.NumTriangles == 0);

			// Log info about the section.
			UE_LOG(LogSkeletalMesh, Log, TEXT("Section %u: Material=%u, %u triangles"),
				SectionIndex,
				Section.MaterialIndex,
				Section.NumTriangles
				);
		}
		if (bHasBadSections)
		{
			FText BadSectionMessage(NSLOCTEXT("UnrealEd", "Error_SkeletalMeshHasBadSections", "Input mesh has a section with no triangles.  This mesh may not render properly."));
			if (OutWarningMessages)
			{
				OutWarningMessages->Add(BadSectionMessage);
				if (OutWarningNames)
				{
					OutWarningNames->Add(FFbxErrors::SkeletalMesh_SectionWithNoTriangle);
				}
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, BadSectionMessage);
			}
		}

		if (bTooManyVerts)
		{
			FText TooManyVertsMessage(NSLOCTEXT("UnrealEd", "Error_SkeletalMeshTooManyVertices", "Input mesh has too many vertices.  The generated mesh will be corrupt!  Consider adding extra materials to split up the source mesh into smaller chunks."));

			if (OutWarningMessages)
			{
				OutWarningMessages->Add(TooManyVertsMessage);
				if (OutWarningNames)
				{
					OutWarningNames->Add(FFbxErrors::SkeletalMesh_TooManyVertices);
				}
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, TooManyVertsMessage);
			}
		}
	}

	return true;
}

static bool NonOpaqueMaterialPredicate(UStaticMeshComponent* InMesh)
{
	TArray<UMaterialInterface*> OutMaterials;
	InMesh->GetUsedMaterials(OutMaterials);
	for (auto Material : OutMaterials)
	{
		if (Material == nullptr || Material->GetBlendMode() != BLEND_Opaque)
		{
			return true;
		}
	}

	return false;
}

static FIntPoint ConditionalImageResize(const FIntPoint& SrcSize, const FIntPoint& DesiredSize, TArray<FColor>& InOutImage, bool bLinearSpace)
{
	const int32 NumDesiredSamples = DesiredSize.X*DesiredSize.Y;
	if (InOutImage.Num() && InOutImage.Num() != NumDesiredSamples)
	{
		check(InOutImage.Num() == SrcSize.X*SrcSize.Y);
		TArray<FColor> OutImage;
		if (NumDesiredSamples > 0)
		{
			FImageUtils::ImageResize(SrcSize.X, SrcSize.Y, InOutImage, DesiredSize.X, DesiredSize.Y, OutImage, bLinearSpace);
		}
		Exchange(InOutImage, OutImage);
		return DesiredSize;
	}

	return SrcSize;
}

static void RetrieveValidStaticMeshComponentsForMerging(AActor* InActor, TArray<UStaticMeshComponent*>& OutComponents)
{
	TInlineComponentArray<UStaticMeshComponent*> Components;
	InActor->GetComponents<UStaticMeshComponent>(Components);
	// TODO: support derived classes from static component
	Components.RemoveAll([](UStaticMeshComponent* Val){ return !(Val->GetClass() == UStaticMeshComponent::StaticClass() || Val->IsA(USplineMeshComponent::StaticClass())); });

	// TODO: support non-opaque materials
	//Components.RemoveAll(&NonOpaqueMaterialPredicate);
	OutComponents.Append(Components);
}

void FMeshUtilities::CreateProxyMesh(const TArray<AActor*>& InActors, const struct FMeshProxySettings& InMeshProxySettings, UPackage* InOuter, const FString& InProxyBasePackageName, const FGuid InGuid, FCreateProxyDelegate InProxyCreatedDelegate, const bool bAllowAsync, const float ScreenAreaSize)
{
	FScopedSlowTask MainTask(100, (LOCTEXT("MeshUtilities_CreateProxyMesh", "Creating Proxy Mesh")));
	MainTask.MakeDialog();

	// Error/warning checking for input
	if (MeshMerging == NULL)
	{
		UE_LOG(LogMeshUtilities, Log, TEXT("No automatic mesh merging module available"));
		return;
	}

	// Check that the delegate has a func-ptr bound to it
	if (!InProxyCreatedDelegate.IsBound())
	{
		UE_LOG(LogMeshUtilities, Log, TEXT("Invalid (unbound) delegate for returning generated proxy mesh"));
		return;
	}

	// No actors given as input
	if (InActors.Num() == 0)
	{
		UE_LOG(LogMeshUtilities, Log, TEXT("No actors specified to generate a proxy mesh for"));
		return;
	}

	// Base asset name for a new assets
	// In case outer is null ProxyBasePackageName has to be long package name
	if (InOuter == nullptr && FPackageName::IsShortPackageName(InProxyBasePackageName))
	{
		UE_LOG(LogMeshUtilities, Warning, TEXT("Invalid long package name: '%s'."), *InProxyBasePackageName);
		return;
	}
	
	MainTask.EnterProgressFrame(10.0f);

	// Retrieve static mesh components valid for merging from the given set of actors	
	TArray<UStaticMeshComponent*> ComponentsToMerge;
	{
		FScopedSlowTask SubTask(InActors.Num(), (LOCTEXT("MeshUtilities_CreateProxyMesh_CollectStaticMeshComponents", "Collecting StaticMeshComponents")));
		// Collect components to merge
		for (AActor* Actor : InActors)
		{
			RetrieveValidStaticMeshComponentsForMerging(Actor, ComponentsToMerge);
			SubTask.EnterProgressFrame(1.0f);
		}
	}

	MainTask.EnterProgressFrame(10.0f);
	
	// Check if there are actually any static mesh components to merge
	if (ComponentsToMerge.Num() == 0)
	{
		UE_LOG(LogMeshUtilities, Log, TEXT("No valid static mesh components found in given set of Actors"));
		return;
	}

	
	typedef FIntPoint FMeshIdAndLOD;
	TArray<FRawMeshExt> SourceMeshes;
	TArray<UMaterialInterface*> GlobalUniqueMaterialList;
	TMap<FMeshIdAndLOD, TArray<int32>> GlobalMaterialMap;
	static const int32 ProxyMeshTargetLODLevel = 0;

	FBoxSphereBounds EstimatedBounds(ForceInitToZero);
	for (const UStaticMeshComponent* StaticMeshComponent : ComponentsToMerge)
	{
		EstimatedBounds = EstimatedBounds + StaticMeshComponent->Bounds;
	}

	static const float FOVRad = 90.0f * (float)PI / 360.0f;
	static const FMatrix ProjectionMatrix = FPerspectiveMatrix(FOVRad, 1920, 1080, 0.01f);
	FHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
	IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
	float EstimatedDistance = Utilities->CalculateDrawDistanceFromScreenSize(EstimatedBounds.SphereRadius, ScreenAreaSize, ProjectionMatrix);
	
	// Retrieve mesh / material data
	for (const UStaticMeshComponent* StaticMeshComponent : ComponentsToMerge)
	{
		TArray<int32> StaticMeshGlobalMaterialMap;
		FRawMesh* RawMesh = new FRawMesh();
		FMemory::Memzero(RawMesh, sizeof(FRawMesh));
		
		const int32 ProxyMeshSourceLODLevel = InMeshProxySettings.bCalculateCorrectLODModel ? Utilities->GetLODLevelForScreenAreaSize(StaticMeshComponent, Utilities->CalculateScreenSizeFromDrawDistance(StaticMeshComponent->Bounds.SphereRadius, ProjectionMatrix, EstimatedDistance)) : 0;
		// Proxy meshes should always propagate vertex colours for material baking
		static const bool bPropagateVertexColours = true;

		const bool bValidRawMesh = ConstructRawMesh(StaticMeshComponent, ProxyMeshSourceLODLevel, bPropagateVertexColours, *RawMesh, GlobalUniqueMaterialList, StaticMeshGlobalMaterialMap);

		if ( bValidRawMesh )
		{
			// Add constructed raw mesh to source mesh array
			const int32 SourceMeshIndex = SourceMeshes.AddZeroed();
			SourceMeshes[SourceMeshIndex].MeshLODData[ProxyMeshTargetLODLevel].RawMesh = RawMesh;
			SourceMeshes[SourceMeshIndex].bShouldExportLOD[ProxyMeshSourceLODLevel] = true;
			SourceMeshes[SourceMeshIndex].ExportLODIndex = ProxyMeshSourceLODLevel;

			// Append retrieved materials for this static mesh component to the global material map
			GlobalMaterialMap.Add(FMeshIdAndLOD(SourceMeshIndex, ProxyMeshTargetLODLevel), StaticMeshGlobalMaterialMap);
		}
	}

	if (SourceMeshes.Num() == 0)
	{
		UE_LOG(LogMeshUtilities, Log, TEXT("No valid (or completely culled) raw meshes constructed from static mesh components"));
		return;
	}

	TArray<bool> MeshShouldBakeVertexData;
	TMap<FMeshIdAndLOD, TArray<int32> > NewGlobalMaterialMap;
	TArray<UMaterialInterface*> NewGlobalUniqueMaterialList;
	FMaterialUtilities::RemapUniqueMaterialIndices(
		GlobalUniqueMaterialList,
		SourceMeshes,
		GlobalMaterialMap,
		InMeshProxySettings.MaterialSettings,
		true, // Always need vertex data for baking materials
		true, // Always want to merge materials
		MeshShouldBakeVertexData,
		NewGlobalMaterialMap,
		NewGlobalUniqueMaterialList);
	// Use shared material data.
	Exchange(GlobalMaterialMap, NewGlobalMaterialMap);
	Exchange(GlobalUniqueMaterialList, NewGlobalUniqueMaterialList);

	// Flatten Materials
	TArray<FFlattenMaterial> FlattenedMaterials;
	FlattenMaterialsWithMeshData(GlobalUniqueMaterialList, SourceMeshes, GlobalMaterialMap, MeshShouldBakeVertexData, InMeshProxySettings.MaterialSettings, FlattenedMaterials);

	for (FFlattenMaterial& InMaterial : FlattenedMaterials)
	{
		FMaterialUtilities::OptimizeFlattenMaterial(InMaterial);
	}

	//For each raw mesh, re-map the material indices from Local to Global material indices space
	for (int32 RawMeshIndex = 0; RawMeshIndex < SourceMeshes.Num(); ++RawMeshIndex)
	{
		const TArray<int32>& GlobalMaterialIndices = *GlobalMaterialMap.Find(FMeshIdAndLOD(RawMeshIndex, ProxyMeshTargetLODLevel));
		TArray<int32>& MaterialIndices = SourceMeshes[RawMeshIndex].MeshLODData[ProxyMeshTargetLODLevel].RawMesh->FaceMaterialIndices;
		int32 MaterialIndicesCount = MaterialIndices.Num();

		for (int32 TriangleIndex = 0; TriangleIndex < MaterialIndicesCount; ++TriangleIndex)
		{
			int32 LocalMaterialIndex = MaterialIndices[TriangleIndex];
			int32 GlobalMaterialIndex = GlobalMaterialIndices[LocalMaterialIndex];

			//Assign the new material index to the raw mesh
			MaterialIndices[TriangleIndex] = GlobalMaterialIndex;
		}
	}

	// Build proxy mesh
	MainTask.EnterProgressFrame(10.0f);
	
	// Landscape culling
	TArray<FRawMesh*> LandscapeRawMeshes;
	if (InMeshProxySettings.bUseLandscapeCulling)
	{
		// Extract landscape proxies from the world
		TArray<ALandscapeProxy*> LandscapeActors;		
		UWorld* InWorld = InActors.Num() ? InActors[0]->GetWorld() : nullptr;

		uint32 MaxLandscapeExportLOD = 0;
		if (InWorld->IsValidLowLevel())
		{
			for (FConstLevelIterator Iterator = InWorld->GetLevelIterator(); Iterator; ++Iterator)
			{
				for (AActor* Actor : (*Iterator)->Actors)
				{
					if (Actor)
					{
						ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor);
						if (LandscapeProxy && LandscapeProxy->bUseLandscapeForCullingInvisibleHLODVertices)
						{
							// Retrieve highest landscape LOD level possible
							MaxLandscapeExportLOD = FMath::Max(MaxLandscapeExportLOD, FMath::CeilLogTwo(LandscapeProxy->SubsectionSizeQuads + 1) - 1);
							LandscapeActors.Add(LandscapeProxy);
						}
					}
				}
			}
		}

		// Setting determines the precision at which we should export the landscape for culling (highest, half or lowest)
		const uint32 LandscapeExportLOD = ((float)MaxLandscapeExportLOD * (0.5f * (float)InMeshProxySettings.LandscapeCullingPrecision));
		for (ALandscapeProxy* Landscape : LandscapeActors)
		{
			// Export the landscape to raw mesh format
			FRawMesh* LandscapeRawMesh = new FRawMesh();
			FBoxSphereBounds LandscapeBounds = EstimatedBounds;
			Landscape->ExportToRawMesh(LandscapeExportLOD, *LandscapeRawMesh, LandscapeBounds);
			if (LandscapeRawMesh->VertexPositions.Num())
			{
				LandscapeRawMeshes.Add(LandscapeRawMesh);
			}
		}
	}	
	
	// Allocate merge complete data
	FMergeCompleteData* Data = new FMergeCompleteData();
	Data->InOuter = InOuter;
	Data->InProxySettings = InMeshProxySettings;
	Data->ProxyBasePackageName = InProxyBasePackageName;
	Data->CallbackDelegate = InProxyCreatedDelegate;
	
	// Add this proxy job to map	
	Processor->AddProxyJob(InGuid, Data);

	// We are only using LOD level 0 (ProxyMeshTargetLODLevel)
	TArray<FMeshMergeData> MergeData;
	for (FRawMeshExt& SourceMesh : SourceMeshes)
	{
		MergeData.Add(SourceMesh.MeshLODData[ProxyMeshTargetLODLevel]);
	}

	// Populate landscape clipping geometry
	for (FRawMesh* RawMesh : LandscapeRawMeshes)
	{
		FMeshMergeData ClipData;
		ClipData.bIsClippingMesh = true;
		ClipData.RawMesh = RawMesh;
		MergeData.Add(ClipData);
	}

	// Choose Simplygon Swarm (if available) or local proxy lod method
	if (DistributedMeshMerging != nullptr && GetDefault<UEditorPerProjectUserSettings>()->bUseSimplygonSwarm && bAllowAsync)
	{
		DistributedMeshMerging->ProxyLOD(MergeData, Data->InProxySettings, FlattenedMaterials, InGuid);
	}
	else
	{
		MeshMerging->ProxyLOD(MergeData, Data->InProxySettings, FlattenedMaterials, InGuid);
		Processor->Tick(0); // make sure caller gets merging results
	}
	
	for (FMeshMergeData& DataToRelease : MergeData)
	{
		DataToRelease.ReleaseData();
	}
}

void FMeshUtilities::CreateProxyMesh(const TArray<AActor*>& Actors, const struct FMeshProxySettings& InProxySettings, UPackage* InOuter, const FString& ProxyBasePackageName, TArray<UObject*>& OutAssetsToSync, FVector& OutProxyLocation)
{
	CreateProxyMesh(Actors, InProxySettings, InOuter, ProxyBasePackageName, OutAssetsToSync);
}

void FMeshUtilities::CreateProxyMesh(const TArray<AActor*>& Actors, const struct FMeshProxySettings& InProxySettings, UPackage* InOuter, const FString& ProxyBasePackageName, TArray<UObject*>& OutAssetsToSync, const float ScreenAreaSize)
{
	FCreateProxyDelegate Delegate;

	FGuid JobGuid = FGuid::NewGuid();
	Delegate.BindLambda(
		[&](const FGuid Guid, TArray<UObject*>& InAssetsToSync)
	{
		if (JobGuid == Guid)
		{
			OutAssetsToSync.Append(InAssetsToSync);
		}
	}
	);

	CreateProxyMesh(Actors, InProxySettings, InOuter, ProxyBasePackageName, JobGuid, Delegate, false, ScreenAreaSize);
}

void FMeshUtilities::FlattenMaterialsWithMeshData(TArray<UMaterialInterface*>& InMaterials, TArray<FRawMeshExt>& InSourceMeshes, TMap<FMeshIdAndLOD, TArray<int32>>& InMaterialIndexMap, TArray<bool>& InMeshShouldBakeVertexData, const FMaterialProxySettings &InMaterialProxySettings, TArray<FFlattenMaterial> &OutFlattenedMaterials) const
{
	// Prepare container for cached shaders.
	TMap<UMaterialInterface*, FExportMaterialProxyCache> CachedShaders;
	CachedShaders.Empty(InMaterials.Num());

	bool bDitheredLODTransition = false;

	for (int32 MaterialIndex = 0; MaterialIndex < InMaterials.Num(); MaterialIndex++)
	{
		UMaterialInterface* CurrentMaterial = InMaterials[MaterialIndex];

		// Store if any material uses dithered transitions
		bDitheredLODTransition |= CurrentMaterial->IsDitheredLODTransition();

		// Check if we already have cached compiled shader for this material.
		FExportMaterialProxyCache* CachedShader = CachedShaders.Find(CurrentMaterial);
		if (CachedShader == nullptr)
		{
			CachedShader = &CachedShaders.Add(CurrentMaterial);
		}

		FFlattenMaterial FlattenMaterial = FMaterialUtilities::CreateFlattenMaterialWithSettings(InMaterialProxySettings);

		/* Find a mesh which uses the current material. Materials using vertex data are added for each individual mesh using it,
		which is why baking down the materials like this works. :) */
		int32 UsedMeshIndex = 0;
		int32 LocalMaterialIndex = 0;
		int32 LocalTextureBoundIndex = 0;
		FMeshMergeData* MergeData = nullptr;
		for (int32 MeshIndex = 0; MeshIndex < InSourceMeshes.Num() && MergeData == nullptr; MeshIndex++)
		{
			const int32 LODIndex = InSourceMeshes[MeshIndex].ExportLODIndex;
			if (InSourceMeshes[MeshIndex].MeshLODData[LODIndex].RawMesh->VertexPositions.Num())
			{
				const TArray<int32>& GlobalMaterialIndices = *InMaterialIndexMap.Find(FMeshIdAndLOD(MeshIndex, LODIndex));
				for (LocalMaterialIndex = 0; LocalMaterialIndex < GlobalMaterialIndices.Num(); LocalMaterialIndex++)
				{
					if (GlobalMaterialIndices[LocalMaterialIndex] == MaterialIndex)
					{
						UsedMeshIndex = MeshIndex;
						MergeData = &InSourceMeshes[MeshIndex].MeshLODData[LODIndex];
						LocalTextureBoundIndex = LocalMaterialIndex;		
						break;
					}
				}
			}
			else
			{
				break;
			}
		}

		// If there is specific vertex data available and used in the material we should generate non-overlapping UVs
		if (MergeData && InMeshShouldBakeVertexData[UsedMeshIndex])
		{
			// Generate new non-overlapping texture coordinates for mesh
			if (MergeData->TexCoordBounds.Num() == 0)
			{
				// Calculate the max bounds for this raw mesh 
				CalculateTextureCoordinateBoundsForRawMesh(*MergeData->RawMesh, MergeData->TexCoordBounds);

				// Generate unique UVs
				GenerateUniqueUVsForStaticMesh(*MergeData->RawMesh, InMaterialProxySettings.TextureSize.GetMax(), MergeData->NewUVs);
			}

			// Export the material using mesh data to support vertex based material properties
			FMaterialUtilities::ExportMaterial(
				CurrentMaterial,
				MergeData->RawMesh,
				LocalMaterialIndex,
				MergeData->TexCoordBounds[LocalTextureBoundIndex],
				MergeData->NewUVs,
				FlattenMaterial,
				CachedShader);
		}
		else
		{		
			// Export the material without vertex data
			FMaterialUtilities::ExportMaterial(
				CurrentMaterial,
				FlattenMaterial,
				CachedShader);
		}

		// Fill flatten material samples alpha values with 255 (for saving out textures correctly for Simplygon Swarm)
		FlattenMaterial.FillAlphaValues(255);

		// Add flattened material to outgoing array
		OutFlattenedMaterials.Add(FlattenMaterial);

		// Check if this material will be used later. If not - release shader.
		bool bMaterialStillUsed = false;
		for (int32 Index = MaterialIndex + 1; Index < InMaterials.Num(); Index++)
		{
			if (InMaterials[Index] == CurrentMaterial)
			{
				bMaterialStillUsed = true;
				break;
			}
		}
		if (!bMaterialStillUsed)
		{
			CachedShader->Release();
		}
	}

	if (OutFlattenedMaterials.Num() > 1)
	{
		// Dither transition fix-up
		for (FFlattenMaterial& FlatMaterial : OutFlattenedMaterials)
		{
			FlatMaterial.bDitheredLODTransition = bDitheredLODTransition;
		}

		// Start with determining maximum emissive scale	
		float MaxEmissiveScale = 0.0f;
		for (FFlattenMaterial& FlatMaterial : OutFlattenedMaterials)
		{
			if (FlatMaterial.EmissiveSamples.Num())
			{
				if (FlatMaterial.EmissiveScale > MaxEmissiveScale)
				{
					MaxEmissiveScale = FlatMaterial.EmissiveScale;
				}
			}
		}

		if (MaxEmissiveScale > 0.001f)
		{
			// Rescale all materials.
			for (FFlattenMaterial& FlatMaterial : OutFlattenedMaterials)
			{
				const float Scale = FlatMaterial.EmissiveScale / MaxEmissiveScale;
				if (FMath::Abs(Scale - 1.0f) < 0.01f)
				{
					// Difference is not noticeable for this material, or this material has maximal emissive level.
					continue;
				}
				// Rescale emissive data.
				for (int32 PixelIndex = 0; PixelIndex < FlatMaterial.EmissiveSamples.Num(); PixelIndex++)
				{
					FColor& C = FlatMaterial.EmissiveSamples[PixelIndex];
					C.R = FMath::RoundToInt(C.R * Scale);
					C.G = FMath::RoundToInt(C.G * Scale);
					C.B = FMath::RoundToInt(C.B * Scale);
				}

				// Update emissive scale to maximum 
				FlatMaterial.EmissiveScale = MaxEmissiveScale;
			}
		}
	}
}

// Exports static mesh LOD render data to a RawMesh
static void ExportStaticMeshLOD(const FStaticMeshLODResources& StaticMeshLOD, FRawMesh& OutRawMesh)
{
	const int32 NumWedges = StaticMeshLOD.IndexBuffer.GetNumIndices();
	const int32 NumVertexPositions = StaticMeshLOD.PositionVertexBuffer.GetNumVertices();
	const int32 NumFaces = NumWedges / 3;

	// Indices
	StaticMeshLOD.IndexBuffer.GetCopy(OutRawMesh.WedgeIndices);

	// Vertex positions
	if (NumVertexPositions > 0)
	{
		OutRawMesh.VertexPositions.Empty(NumVertexPositions);
		for (int32 PosIdx = 0; PosIdx < NumVertexPositions; ++PosIdx)
		{
			FVector Pos = StaticMeshLOD.PositionVertexBuffer.VertexPosition(PosIdx);
			OutRawMesh.VertexPositions.Add(Pos);
		}
	}

	// Vertex data
	if (StaticMeshLOD.VertexBuffer.GetNumVertices() > 0)
	{
		OutRawMesh.WedgeTangentX.Empty(NumWedges);
		OutRawMesh.WedgeTangentY.Empty(NumWedges);
		OutRawMesh.WedgeTangentZ.Empty(NumWedges);

		const int32 NumTexCoords = StaticMeshLOD.VertexBuffer.GetNumTexCoords();
		for (int32 TexCoodIdx = 0; TexCoodIdx < NumTexCoords; ++TexCoodIdx)
		{
			OutRawMesh.WedgeTexCoords[TexCoodIdx].Empty(NumWedges);
		}

		for (int32 WedgeIndex : OutRawMesh.WedgeIndices)
		{
			FVector WedgeTangentX = StaticMeshLOD.VertexBuffer.VertexTangentX(WedgeIndex);
			FVector WedgeTangentY = StaticMeshLOD.VertexBuffer.VertexTangentY(WedgeIndex);
			FVector WedgeTangentZ = StaticMeshLOD.VertexBuffer.VertexTangentZ(WedgeIndex);
			OutRawMesh.WedgeTangentX.Add(WedgeTangentX);
			OutRawMesh.WedgeTangentY.Add(WedgeTangentY);
			OutRawMesh.WedgeTangentZ.Add(WedgeTangentZ);

			for (int32 TexCoodIdx = 0; TexCoodIdx < NumTexCoords; ++TexCoodIdx)
			{
				FVector2D WedgeTexCoord = StaticMeshLOD.VertexBuffer.GetVertexUV(WedgeIndex, TexCoodIdx);
				OutRawMesh.WedgeTexCoords[TexCoodIdx].Add(WedgeTexCoord);
			}
		}
	}

	// Vertex colors
	if (StaticMeshLOD.ColorVertexBuffer.GetNumVertices() > 0)
	{
		OutRawMesh.WedgeColors.Empty(NumWedges);
		for (int32 WedgeIndex : OutRawMesh.WedgeIndices)
		{
			FColor VertexColor = StaticMeshLOD.ColorVertexBuffer.VertexColor(WedgeIndex);
			OutRawMesh.WedgeColors.Add(VertexColor);
		}
	}

	// Materials
	{
		OutRawMesh.FaceMaterialIndices.Empty(NumFaces);
		OutRawMesh.FaceMaterialIndices.SetNumZeroed(NumFaces);

		for (const FStaticMeshSection& Section : StaticMeshLOD.Sections)
		{
			uint32 FirstTriangle = Section.FirstIndex / 3;
			for (uint32 TriangleIndex = 0; TriangleIndex < Section.NumTriangles; ++TriangleIndex)
			{
				OutRawMesh.FaceMaterialIndices[FirstTriangle + TriangleIndex] = Section.MaterialIndex;
			}
		}
	}

	// Smoothing masks
	{
		OutRawMesh.FaceSmoothingMasks.Empty(NumFaces);
		OutRawMesh.FaceSmoothingMasks.SetNumUninitialized(NumFaces);

		for (auto& SmoothingMask : OutRawMesh.FaceSmoothingMasks)
		{
			SmoothingMask = 1;
		}
	}
}



const bool IsLandscapeHit(const FVector& RayOrigin, const FVector& RayEndPoint, const UWorld* World, const TArray<ALandscapeProxy*>& LandscapeProxies, FVector& OutHitLocation)
{
	static FName TraceTag = FName(TEXT("LandscapeTrace"));
	TArray<FHitResult> Results;
	// Each landscape component has 2 collision shapes, 1 of them is specific to landscape editor
	// Trace only ECC_Visibility channel, so we do hit only Editor specific shape
	World->LineTraceMultiByObjectType(Results, RayOrigin, RayEndPoint, FCollisionObjectQueryParams(ECollisionChannel::ECC_Visibility), FCollisionQueryParams(TraceTag, true));

	bool bHitLandscape = false;

	for (const FHitResult& HitResult : Results)
	{
		ULandscapeHeightfieldCollisionComponent* CollisionComponent = Cast<ULandscapeHeightfieldCollisionComponent>(HitResult.Component.Get());
		if (CollisionComponent)
		{
			ALandscapeProxy* HitLandscape = CollisionComponent->GetLandscapeProxy();
			if (HitLandscape && LandscapeProxies.Contains(HitLandscape))
			{
				// Could write a correct clipping algorithm, that clips the triangle to hit location
				OutHitLocation = HitLandscape->LandscapeActorToWorld().InverseTransformPosition(HitResult.Location);
				// Above landscape so visible
				bHitLandscape = true;
			}
		}
	}

	return bHitLandscape;
}


void CullTrianglesFromVolumesAndUnderLandscapes(const UStaticMeshComponent* InMeshComponent, FRawMesh &OutRawMesh)
{
	UWorld* World = InMeshComponent->GetWorld();
	TArray<ALandscapeProxy*> Landscapes;
	TArray<AHLODMeshCullingVolume*> CullVolumes;

	FBox ComponentBox(InMeshComponent->Bounds.Origin - InMeshComponent->Bounds.BoxExtent, InMeshComponent->Bounds.Origin + InMeshComponent->Bounds.BoxExtent);

	for (ULevel* Level : World->GetLevels())
	{
		for (AActor* Actor : Level->Actors)
		{
			ALandscape* Proxy = Cast<ALandscape>(Actor);
			if (Proxy && Proxy->bUseLandscapeForCullingInvisibleHLODVertices)
			{
				FVector Origin, Extent;
				Proxy->GetActorBounds(false, Origin, Extent);
				FBox LandscapeBox(Origin - Extent, Origin + Extent);

				// Ignore Z axis for 2d bounds check
				if (LandscapeBox.IntersectXY(ComponentBox))
				{
					Landscapes.Add(Proxy->GetLandscapeActor());
				}
			}

			// Check for culling volumes
			AHLODMeshCullingVolume* Volume = Cast<AHLODMeshCullingVolume>(Actor);
			if (Volume)
			{
				// If the mesh's bounds intersect with the volume there is a possibility of culling
				const bool bIntersecting = Volume->EncompassesPoint(InMeshComponent->Bounds.Origin, InMeshComponent->Bounds.SphereRadius, nullptr);
				if (bIntersecting)
				{
					CullVolumes.Add(Volume);
				}
			}
		}
	}

	TArray<bool> VertexVisible;
	VertexVisible.AddZeroed(OutRawMesh.VertexPositions.Num());
	int32 Index = 0;

	for (const FVector& Position : OutRawMesh.VertexPositions)
	{
		// Start with setting visibility to true on all vertices
		VertexVisible[Index] = true;

		// Check if this vertex is culled due to being underneath a landscape
		if (Landscapes.Num() > 0)
		{
			bool bVertexWithinLandscapeBounds = false;

			for (ALandscapeProxy* Proxy : Landscapes)
			{
				FVector Origin, Extent;
				Proxy->GetActorBounds(false, Origin, Extent);
				FBox LandscapeBox(Origin - Extent, Origin + Extent);
				bVertexWithinLandscapeBounds |= LandscapeBox.IsInsideXY(Position);
			}

			if (bVertexWithinLandscapeBounds)
			{
				const FVector Start = Position;
				FVector End = Position - (WORLD_MAX * FVector::UpVector);
				FVector OutHit;
				const bool IsAboveLandscape = IsLandscapeHit(Start, End, World, Landscapes, OutHit);

				End = Position + (WORLD_MAX * FVector::UpVector);
				const bool IsUnderneathLandscape = IsLandscapeHit(Start, End, World, Landscapes, OutHit);

				// Vertex is visible when above landscape (with actual landscape underneath) or if there is no landscape beneath or above the vertex (falls outside of landscape bounds)
				VertexVisible[Index] = (IsAboveLandscape && !IsUnderneathLandscape) || (!IsAboveLandscape && !IsUnderneathLandscape);
			}
		}

		// Volume culling	
		for (AHLODMeshCullingVolume* Volume : CullVolumes)
		{
			const bool bVertexIsInsideVolume = Volume->EncompassesPoint(Position, 0.0f, nullptr);
			if (bVertexIsInsideVolume)
			{
				// Inside a culling volume so invisible
				VertexVisible[Index] = false;
			}
		}

		Index++;
	}


	// We now know which vertices are below the landscape
	TArray<bool> TriangleVisible;
	int32 NumTriangles = OutRawMesh.WedgeIndices.Num() / 3;
	TriangleVisible.AddZeroed(NumTriangles);

	bool bCreateNewMesh = false;

	// Determine which triangles of the mesh are visible
	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; TriangleIndex++)
	{
		bool AboveLandscape = false;

		for (int32 WedgeIndex = 0; WedgeIndex < 3; ++WedgeIndex)
		{
			AboveLandscape |= VertexVisible[OutRawMesh.WedgeIndices[(TriangleIndex * 3) + WedgeIndex]];
		}
		TriangleVisible[TriangleIndex] = AboveLandscape;
		bCreateNewMesh |= !AboveLandscape;

	}

	// Check whether or not we have to create a new mesh
	if (bCreateNewMesh)
	{
		FRawMesh NewRawMesh;
		TMap<int32, int32> VertexRemapping;

		// Fill new mesh with data only from visible triangles
		for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
		{
			if (!TriangleVisible[TriangleIndex])
				continue;

			for (int32 WedgeIndex = 0; WedgeIndex < 3; ++WedgeIndex)
			{
				int32 OldIndex = OutRawMesh.WedgeIndices[(TriangleIndex * 3) + WedgeIndex];

				int32 NewIndex;

				int32* RemappedIndex = VertexRemapping.Find(Index);
				if (RemappedIndex)
				{
					NewIndex = *RemappedIndex;
				}
				else
				{
					NewIndex = NewRawMesh.VertexPositions.Add(OutRawMesh.VertexPositions[OldIndex]);
					VertexRemapping.Add(OldIndex, NewIndex);
				}

				NewRawMesh.WedgeIndices.Add(NewIndex);
				if (OutRawMesh.WedgeColors.Num()) NewRawMesh.WedgeColors.Add(OutRawMesh.WedgeColors[(TriangleIndex * 3) + WedgeIndex]);
				if (OutRawMesh.WedgeTangentX.Num()) NewRawMesh.WedgeTangentX.Add(OutRawMesh.WedgeTangentX[(TriangleIndex * 3) + WedgeIndex]);
				if (OutRawMesh.WedgeTangentY.Num()) NewRawMesh.WedgeTangentY.Add(OutRawMesh.WedgeTangentY[(TriangleIndex * 3) + WedgeIndex]);
				if (OutRawMesh.WedgeTangentZ.Num()) NewRawMesh.WedgeTangentZ.Add(OutRawMesh.WedgeTangentZ[(TriangleIndex * 3) + WedgeIndex]);

				for (int32 UVIndex = 0; UVIndex < MAX_MESH_TEXTURE_COORDS; ++UVIndex)
				{
					if (OutRawMesh.WedgeTexCoords[UVIndex].Num())
					{
						NewRawMesh.WedgeTexCoords[UVIndex].Add(OutRawMesh.WedgeTexCoords[UVIndex][(TriangleIndex * 3) + WedgeIndex]);
					}
				}
			}

			NewRawMesh.FaceMaterialIndices.Add(OutRawMesh.FaceMaterialIndices[TriangleIndex]);
			NewRawMesh.FaceSmoothingMasks.Add(OutRawMesh.FaceSmoothingMasks[TriangleIndex]);
		}

		OutRawMesh = NewRawMesh;
	}
}

void PropagateSplineDeformationToRawMesh(const USplineMeshComponent* InSplineMeshComponent, FRawMesh &OutRawMesh) 
{
	// Apply spline deformation for each vertex's tangents
	for (int32 iVert = 0; iVert < OutRawMesh.WedgeIndices.Num(); ++iVert)
	{
		uint32 Index = OutRawMesh.WedgeIndices[iVert];
		float& AxisValue = USplineMeshComponent::GetAxisValue(OutRawMesh.VertexPositions[Index], InSplineMeshComponent->ForwardAxis);
		FTransform SliceTransform = InSplineMeshComponent->CalcSliceTransform(AxisValue);

		// Transform tangents first
		if (OutRawMesh.WedgeTangentX.Num())
		{
			OutRawMesh.WedgeTangentX[iVert] = SliceTransform.TransformVector(OutRawMesh.WedgeTangentX[iVert]);
		}

		if (OutRawMesh.WedgeTangentY.Num())
		{
			OutRawMesh.WedgeTangentY[iVert] = SliceTransform.TransformVector(OutRawMesh.WedgeTangentY[iVert]);
		}

		if (OutRawMesh.WedgeTangentZ.Num())
		{
			OutRawMesh.WedgeTangentZ[iVert] = SliceTransform.TransformVector(OutRawMesh.WedgeTangentZ[iVert]);
		}
	}

	// Apply spline deformation for each vertex position
	for (int32 iVert = 0; iVert < OutRawMesh.VertexPositions.Num(); ++iVert)
	{
		float& AxisValue = USplineMeshComponent::GetAxisValue(OutRawMesh.VertexPositions[iVert], InSplineMeshComponent->ForwardAxis);
		FTransform SliceTransform = InSplineMeshComponent->CalcSliceTransform(AxisValue);
		AxisValue = 0.0f;
		OutRawMesh.VertexPositions[iVert] = SliceTransform.TransformPosition(OutRawMesh.VertexPositions[iVert]);
	}
}


void TransformRawMeshVertexData(const FTransform& InTransform, FRawMesh &OutRawMesh )
{
	for (FVector& Vertex : OutRawMesh.VertexPositions)
	{
		Vertex = InTransform.TransformPosition(Vertex);
	}

	for (FVector& TangentX : OutRawMesh.WedgeTangentX)
	{
		TangentX = InTransform.TransformVectorNoScale(TangentX);
	}

	for (FVector& TangentY : OutRawMesh.WedgeTangentY)
	{
		TangentY = InTransform.TransformVectorNoScale(TangentY);
	}

	for (FVector& TangentZ : OutRawMesh.WedgeTangentZ)
	{
		TangentZ = InTransform.TransformVectorNoScale(TangentZ);
	}
	
	const bool bIsMirrored = InTransform.GetDeterminant() < 0.f;
	if (bIsMirrored)
	{
		// Flip faces
		for (int32 FaceIdx = 0; FaceIdx < OutRawMesh.WedgeIndices.Num() / 3; FaceIdx++)
		{
			int32 I0 = FaceIdx * 3 + 0;
			int32 I2 = FaceIdx * 3 + 2;
			Swap(OutRawMesh.WedgeIndices[I0], OutRawMesh.WedgeIndices[I2]);

			// seems like vertex colors and UVs are not indexed, so swap values instead
			if (OutRawMesh.WedgeColors.Num())
			{
				Swap(OutRawMesh.WedgeColors[I0], OutRawMesh.WedgeColors[I2]);
			}

			for (int32 i = 0; i < MAX_MESH_TEXTURE_COORDS; ++i)
			{
				if (OutRawMesh.WedgeTexCoords[i].Num())
				{
					Swap(OutRawMesh.WedgeTexCoords[i][I0], OutRawMesh.WedgeTexCoords[i][I2]);
				}
			}
		}
	}
}


void RecomputeTangentsAndNormalsForRawMesh(bool bRecomputeTangents, bool bRecomputeNormals, const FMeshBuildSettings& InBuildSettings, FRawMesh &OutRawMesh )
{
	const int32 NumWedges = OutRawMesh.WedgeIndices.Num();

	// Dump normals and tangents if we are recomputing them.
	if (bRecomputeTangents)
	{
		OutRawMesh.WedgeTangentX.Empty(NumWedges);
		OutRawMesh.WedgeTangentX.AddZeroed(NumWedges);
		OutRawMesh.WedgeTangentY.Empty(NumWedges);
		OutRawMesh.WedgeTangentY.AddZeroed(NumWedges);
	}

	if (bRecomputeNormals)
	{
		OutRawMesh.WedgeTangentZ.Empty(NumWedges);
		OutRawMesh.WedgeTangentZ.AddZeroed(NumWedges);
	}

	// Compute any missing tangents.
	if (bRecomputeNormals || bRecomputeTangents)
	{
		float ComparisonThreshold = GetComparisonThreshold(InBuildSettings);
		TMultiMap<int32, int32> OverlappingCorners;
		FindOverlappingCorners(OverlappingCorners, OutRawMesh, ComparisonThreshold);

		// Static meshes always blend normals of overlapping corners.
		uint32 TangentOptions = ETangentOptions::BlendOverlappingNormals;
		if (InBuildSettings.bRemoveDegenerates)
		{
			// If removing degenerate triangles, ignore them when computing tangents.
			TangentOptions |= ETangentOptions::IgnoreDegenerateTriangles;
		}
		if (InBuildSettings.bUseMikkTSpace)
		{
			ComputeTangents_MikkTSpace(OutRawMesh, OverlappingCorners, TangentOptions);
		}
		else
		{
			ComputeTangents(OutRawMesh, OverlappingCorners, TangentOptions);
		}
	}

	// At this point the mesh will have valid tangents.
	check(OutRawMesh.WedgeTangentX.Num() == NumWedges);
	check(OutRawMesh.WedgeTangentY.Num() == NumWedges);
	check(OutRawMesh.WedgeTangentZ.Num() == NumWedges);
}

bool FMeshUtilities::ConstructRawMesh(
	const UStaticMeshComponent* InMeshComponent,
	int32 InLODIndex,
	const bool bPropagateVertexColours,
	FRawMesh& OutRawMesh,
	TArray<UMaterialInterface*>& OutUniqueMaterials,
	TArray<int32>& OutGlobalMaterialIndices) const
{
	// Retrieve source static mesh 
	const UStaticMesh* SourceStaticMesh = InMeshComponent->StaticMesh;
	
	if (SourceStaticMesh == NULL)
	{
		UE_LOG(LogMeshUtilities, Warning, TEXT("No static mesh actor found in component %s."), *InMeshComponent->GetName());
		return false;
	}

	if (!SourceStaticMesh->SourceModels.IsValidIndex(InLODIndex))
	{
		UE_LOG(LogMeshUtilities, Log, TEXT("No mesh data found for LOD%d %s."), InLODIndex, *SourceStaticMesh->GetName());
		return false;
	}

	if (!SourceStaticMesh->RenderData->LODResources.IsValidIndex(InLODIndex))
	{
		UE_LOG(LogMeshUtilities, Warning, TEXT("No mesh render data found for LOD%d %s."), InLODIndex, *SourceStaticMesh->GetName());
		return false;
	}

	const FStaticMeshSourceModel& SourceStaticMeshModel = SourceStaticMesh->SourceModels[InLODIndex];

	// Imported meshes will have a filled RawMeshBulkData set
	const bool bImportedMesh = !SourceStaticMeshModel.RawMeshBulkData->IsEmpty();
	// Check whether or not this mesh has been reduced in-engine
	const bool bReducedMesh = (SourceStaticMeshModel.ReductionSettings.PercentTriangles < 1.0f);
	// rying to retrieve rawmesh from SourceStaticMeshModel was giving issues, which causes a mismatch
	const bool bRenderDataMismatch = (InLODIndex > 0);
	
	// Determine whether we load the raw mesh data from (original) import data or from the generated render data resources
	if (bImportedMesh && !InMeshComponent->IsA<USplineMeshComponent>() && !bReducedMesh && !bRenderDataMismatch)
	{
		SourceStaticMeshModel.RawMeshBulkData->LoadRawMesh(OutRawMesh);
	}
	else
	{		
		ExportStaticMeshLOD(SourceStaticMesh->RenderData->LODResources[InLODIndex], OutRawMesh);
	}

	// Make sure the raw mesh is not irreparably malformed.
	if (!OutRawMesh.IsValidOrFixable())
	{
		UE_LOG(LogMeshUtilities, Error, TEXT("Raw mesh (%s) is corrupt for LOD%d."), *SourceStaticMesh->GetName(), InLODIndex);
		return false;
	}
	
	// Handle spline mesh deformation
	if (InMeshComponent->IsA<USplineMeshComponent>())
	{
		const USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(InMeshComponent);
		// Deform raw mesh data according to the Spline Mesh Component's data
		PropagateSplineDeformationToRawMesh(SplineMeshComponent, OutRawMesh);
	}

	// Use build settings from base mesh for LOD entries that was generated inside Editor.
	const FMeshBuildSettings& BuildSettings = bImportedMesh ? SourceStaticMeshModel.BuildSettings : SourceStaticMesh->SourceModels[0].BuildSettings;

	// Transform raw mesh to world space
	FTransform ComponentToWorldTransform = InMeshComponent->ComponentToWorld;
	// Take into account build scale settings only for meshes imported from raw data
	// meshes reconstructed from render data already have build scale applied
	if (bImportedMesh)
	{
		ComponentToWorldTransform.SetScale3D(ComponentToWorldTransform.GetScale3D()*BuildSettings.BuildScale3D);
	}

	// If specified propagate painted vertex colors into our raw mesh
	if (bPropagateVertexColours)
	{
		PropagatePaintedColorsToRawMesh(InMeshComponent, InLODIndex, OutRawMesh);
	}

	// Transform raw mesh vertex data by the Static Mesh Component's component to world transformation	
	TransformRawMeshVertexData(ComponentToWorldTransform, OutRawMesh);	

	// Culling triangles could lead to an entirely empty RawMesh (all vertices culled)
	if (!OutRawMesh.IsValid())
	{
		return false;
	}

	// Figure out if we should recompute normals and tangents. By default generated LODs should not recompute normals
	const bool bIsMirrored = ComponentToWorldTransform.GetDeterminant() < 0.f;
	bool bRecomputeNormals = (bImportedMesh && BuildSettings.bRecomputeNormals) || OutRawMesh.WedgeTangentZ.Num() == 0 || bIsMirrored;
	bool bRecomputeTangents = (bImportedMesh && BuildSettings.bRecomputeTangents) || OutRawMesh.WedgeTangentX.Num() == 0 || OutRawMesh.WedgeTangentY.Num() == 0 || bIsMirrored;

	if (bRecomputeNormals || bRecomputeTangents)
	{
		RecomputeTangentsAndNormalsForRawMesh(bRecomputeTangents, bRecomputeNormals, BuildSettings, OutRawMesh);
	}

	// Retrieving materials
	UMaterialInterface* DefaultMaterial = Cast<UMaterialInterface>(UMaterial::GetDefaultMaterial(MD_Surface));

	//Need to store the unique material indices in order to re-map the material indices in each rawmesh	
	for (const FStaticMeshSection& Section : SourceStaticMesh->RenderData->LODResources[InLODIndex].Sections)
	{
		// Add material and store the material ID
		UMaterialInterface* MaterialToAdd = InMeshComponent->GetMaterial(Section.MaterialIndex);

		if (MaterialToAdd)
		{
			//Need to check if the resource exists			
			FMaterialResource* Resource = MaterialToAdd->GetMaterialResource(GMaxRHIFeatureLevel);
			if (!Resource)
			{
				MaterialToAdd = DefaultMaterial;
			}
		}
		else
		{
			MaterialToAdd = DefaultMaterial;
		}

		const int32 MaterialIdx = OutUniqueMaterials.Add(MaterialToAdd);
		const int32 MaterialMapIdx = OutGlobalMaterialIndices.Add(MaterialIdx);
		
		// Update face material indices?
		if (OutRawMesh.FaceMaterialIndices.Num())
		{
			for (int32& MaterialIndex : OutRawMesh.FaceMaterialIndices)
			{
				if (MaterialIndex == Section.MaterialIndex)
				{
					MaterialIndex = MaterialMapIdx;
				}
			}
		}
	}

	return true;
}

void FMeshUtilities::ExtractMeshDataForGeometryCache(FRawMesh& RawMesh, const FMeshBuildSettings& BuildSettings, TArray<FStaticMeshBuildVertex>& OutVertices, TArray<TArray<uint32> >& OutPerSectionIndices)
{
	int32 NumWedges = RawMesh.WedgeIndices.Num();

	// Figure out if we should recompute normals and tangents. By default generated LODs should not recompute normals
	bool bRecomputeNormals = (BuildSettings.bRecomputeNormals) || RawMesh.WedgeTangentZ.Num() == 0;
	bool bRecomputeTangents = (BuildSettings.bRecomputeTangents) || RawMesh.WedgeTangentX.Num() == 0 || RawMesh.WedgeTangentY.Num() == 0;

	// Dump normals and tangents if we are recomputing them.
	if (bRecomputeTangents)
	{
		RawMesh.WedgeTangentX.Empty(NumWedges);
		RawMesh.WedgeTangentX.AddZeroed(NumWedges);
		RawMesh.WedgeTangentY.Empty(NumWedges);
		RawMesh.WedgeTangentY.AddZeroed(NumWedges);
	}

	if (bRecomputeNormals)
	{
		RawMesh.WedgeTangentZ.Empty(NumWedges);
		RawMesh.WedgeTangentZ.AddZeroed(NumWedges);
	}

	// Compute any missing tangents.
	TMultiMap<int32, int32> OverlappingCorners;
	if (bRecomputeNormals || bRecomputeTangents)
	{
		float ComparisonThreshold = GetComparisonThreshold(BuildSettings);
		FindOverlappingCorners(OverlappingCorners, RawMesh, ComparisonThreshold);

		// Static meshes always blend normals of overlapping corners.
		uint32 TangentOptions = ETangentOptions::BlendOverlappingNormals;
		if (BuildSettings.bRemoveDegenerates)
		{
			// If removing degenerate triangles, ignore them when computing tangents.
			TangentOptions |= ETangentOptions::IgnoreDegenerateTriangles;
		}
		if (BuildSettings.bUseMikkTSpace)
		{
			ComputeTangents_MikkTSpace(RawMesh, OverlappingCorners, TangentOptions);
		}
		else
		{
			ComputeTangents(RawMesh, OverlappingCorners, TangentOptions);
		}
	}

	// At this point the mesh will have valid tangents.
	check(RawMesh.WedgeTangentX.Num() == NumWedges);
	check(RawMesh.WedgeTangentY.Num() == NumWedges);
	check(RawMesh.WedgeTangentZ.Num() == NumWedges);

	TArray<int32> OutWedgeMap;

	int32 MaxMaterialIndex = 1;
	for (int32 FaceIndex = 0; FaceIndex < RawMesh.FaceMaterialIndices.Num(); FaceIndex++)
	{
		MaxMaterialIndex = FMath::Max<int32>(RawMesh.FaceMaterialIndices[FaceIndex], MaxMaterialIndex);
	}

	for (int32 i = 0; i <= MaxMaterialIndex; ++i)
	{
		OutPerSectionIndices.Push(TArray<uint32>());
	}

	BuildStaticMeshVertexAndIndexBuffers(OutVertices, OutPerSectionIndices, OutWedgeMap, RawMesh, OverlappingCorners, KINDA_SMALL_NUMBER, BuildSettings.BuildScale3D);

	if (RawMesh.WedgeIndices.Num() < 100000 * 3)
	{
		CacheOptimizeVertexAndIndexBuffer(OutVertices, OutPerSectionIndices, OutWedgeMap);
		check(OutWedgeMap.Num() == RawMesh.WedgeIndices.Num());
	}
}

/*------------------------------------------------------------------------------
Mesh merging
------------------------------------------------------------------------------*/
bool FMeshUtilities::PropagatePaintedColorsToRawMesh(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FRawMesh& RawMesh) const
{
	UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;

	if (StaticMesh->SourceModels.IsValidIndex(LODIndex) &&
		StaticMeshComponent->LODData.IsValidIndex(LODIndex) &&
		StaticMeshComponent->LODData[LODIndex].OverrideVertexColors != nullptr)
	{
		FColorVertexBuffer& ColorVertexBuffer = *StaticMeshComponent->LODData[LODIndex].OverrideVertexColors;
		FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
		FStaticMeshRenderData& RenderData = *StaticMesh->RenderData;
		FStaticMeshLODResources& RenderModel = RenderData.LODResources[LODIndex];

		if (ColorVertexBuffer.GetNumVertices() == RenderModel.GetNumVertices())
		{	
			int32 NumWedges = RawMesh.WedgeIndices.Num();
			const bool bUseWedgeMap = RenderData.WedgeMap.Num() > 0 && RenderData.WedgeMap.Num() == NumWedges;
			// If we have a wedge map
			if (bUseWedgeMap)
			{
				if (RenderData.WedgeMap.Num() == NumWedges)
				{
					int32 NumExistingColors = RawMesh.WedgeColors.Num();
					if (NumExistingColors < NumWedges)
					{
						RawMesh.WedgeColors.AddUninitialized(NumWedges - NumExistingColors);
					}

					for (int32 i = 0; i < NumWedges; ++i)
					{
						FColor WedgeColor = FColor::White;
						int32 Index = RenderData.WedgeMap[i];
						if (Index != INDEX_NONE)
						{
							WedgeColor = ColorVertexBuffer.VertexColor(Index);
						}

						RawMesh.WedgeColors[i] = WedgeColor;
					}

					return true;
				}
			}
			// No wedge map (this can happen when we poly reduce the LOD for example)
			// Use index buffer directly
			else 
			{
				UE_LOG(LogMeshUtilities, Warning, TEXT("{%s} Wedge map size %d is wrong or empty. Expected %d. Falling back on using index buffer for propagating vertex painting"), *StaticMesh->GetName(), RenderData.WedgeMap.Num(), RawMesh.WedgeIndices.Num());

				RawMesh.WedgeColors.SetNumUninitialized(NumWedges);
				
				if (RawMesh.VertexPositions.Num() == ColorVertexBuffer.GetNumVertices())
				{
					for (int32 i = 0; i < NumWedges; ++i)
					{
						FColor WedgeColor = FColor::White;
						uint32 VertIndex = RawMesh.WedgeIndices[i]; 
						
						if (VertIndex < ColorVertexBuffer.GetNumVertices())
						{
							WedgeColor = ColorVertexBuffer.VertexColor(VertIndex);
						}
						RawMesh.WedgeColors[i] = WedgeColor;
					}

					return true;
				}
			}
		}	
	}

	return false;
}

static void TransformPhysicsGeometry(const FTransform& InTransform, FKAggregateGeom& AggGeom)
{
	for (auto& Elem : AggGeom.SphereElems)
	{
		FTransform ElemTM = Elem.GetTransform();
		Elem.SetTransform(ElemTM*InTransform);
	}

	for (auto& Elem : AggGeom.BoxElems)
	{
		FTransform ElemTM = Elem.GetTransform();
		Elem.SetTransform(ElemTM*InTransform);
	}

	for (auto& Elem : AggGeom.SphylElems)
	{
		FTransform ElemTM = Elem.GetTransform();
		Elem.SetTransform(ElemTM*InTransform);
	}

	for (auto& Elem : AggGeom.ConvexElems)
	{
		FTransform ElemTM = Elem.GetTransform();
		Elem.SetTransform(ElemTM*InTransform);
	}

	// seems like all primitives except Convex need separate scaling pass		
	const FVector Scale3D = InTransform.GetScale3D();
	if (!Scale3D.Equals(FVector(1.f)))
	{
		const float MinPrimSize = KINDA_SMALL_NUMBER;

		for (auto& Elem : AggGeom.SphereElems)
		{
			Elem.ScaleElem(Scale3D, MinPrimSize);
		}

		for (auto& Elem : AggGeom.BoxElems)
		{
			Elem.ScaleElem(Scale3D, MinPrimSize);
		}

		for (auto& Elem : AggGeom.SphylElems)
		{
			Elem.ScaleElem(Scale3D, MinPrimSize);
		}
	}
}

static void ExtractPhysicsGeometry(UStaticMeshComponent* InMeshComponent, FKAggregateGeom& OutAggGeom)
{
	UStaticMesh* SrcMesh = InMeshComponent->StaticMesh;
	if (SrcMesh == nullptr)
	{
		return;
	}

	if (!SrcMesh->BodySetup)
	{
		return;
	}

	OutAggGeom = SrcMesh->BodySetup->AggGeom;

	// we are not owner of this stuff
	OutAggGeom.RenderInfo = nullptr;
	for (auto& Elem : OutAggGeom.ConvexElems)
	{
		Elem.ConvexMesh = nullptr;
		Elem.ConvexMeshNegX = nullptr;
	}

	// Transform geometry to world space
	FTransform CtoM = InMeshComponent->ComponentToWorld;
	TransformPhysicsGeometry(CtoM, OutAggGeom);
}

void FMeshUtilities::CalculateTextureCoordinateBoundsForRawMesh(const FRawMesh& InRawMesh, TArray<FBox2D>& OutBounds) const
{
	const int32 NumWedges = InRawMesh.WedgeIndices.Num();
	const int32 NumTris = NumWedges / 3;

	OutBounds.Empty();
	int32 WedgeIndex = 0;
	for (int32 TriIndex = 0; TriIndex < NumTris; TriIndex++)
	{
		int MaterialIndex = InRawMesh.FaceMaterialIndices[TriIndex];
		if (OutBounds.Num() <= MaterialIndex)
			OutBounds.SetNumZeroed(MaterialIndex + 1);
		{
			for (int32 CornerIndex = 0; CornerIndex < 3; CornerIndex++, WedgeIndex++)
			{
				OutBounds[MaterialIndex] += InRawMesh.WedgeTexCoords[0][WedgeIndex];
			}
		}
	}
}

void FMeshUtilities::CalculateTextureCoordinateBoundsForSkeletalMesh(const FStaticLODModel& LODModel, TArray<FBox2D>& OutBounds) const
{
	TArray<FSoftSkinVertex> Vertices;
	FMultiSizeIndexContainerData IndexData;
	LODModel.GetVertices(Vertices);
	LODModel.MultiSizeIndexContainer.GetIndexBufferData(IndexData);

#if WITH_APEX_CLOTHING
	const uint32 SectionCount = (uint32)LODModel.NumNonClothingSections();
#else
	const uint32 SectionCount = LODModel.Sections.Num();
#endif // #if WITH_APEX_CLOTHING

	check(OutBounds.Num() != 0);

	for (uint32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const FSkelMeshSection& Section = LODModel.Sections[SectionIndex];
		const uint32 FirstIndex = Section.BaseIndex;
		const uint32 LastIndex = FirstIndex + Section.NumTriangles * 3;
		const int32 MaterialIndex = Section.MaterialIndex;

		if (OutBounds.Num() <= MaterialIndex)
		{
			OutBounds.SetNumZeroed(MaterialIndex + 1);
		}

		for (uint32 Index = FirstIndex; Index < LastIndex; ++Index)
		{
			uint32 VertexIndex = IndexData.Indices[Index];
			FSoftSkinVertex& Vertex = Vertices[VertexIndex];

			FVector2D TexCoord = Vertex.UVs[0];
			OutBounds[MaterialIndex] += TexCoord;
		}
	}
}

static void CopyTextureRect(const FColor* Src, const FIntPoint& SrcSize, FColor* Dst, const FIntPoint& DstSize, const FIntPoint& DstPos)
{
	int32 RowLength = SrcSize.X*sizeof(FColor);
	FColor* RowDst = Dst + DstSize.X*DstPos.Y;
	const FColor* RowSrc = Src;

	for (int32 RowIdx = 0; RowIdx < SrcSize.Y; ++RowIdx)
	{
		FMemory::Memcpy(RowDst + DstPos.X, RowSrc, RowLength);

		RowDst += DstSize.X;
		RowSrc += SrcSize.X;
	}
}

static void SetTextureRect(const FColor& ColorValue, const FIntPoint& SrcSize, FColor* Dst, const FIntPoint& DstSize, const FIntPoint& DstPos)
{
	FColor* RowDst = Dst + DstSize.X*DstPos.Y;

	for (int32 RowIdx = 0; RowIdx < SrcSize.Y; ++RowIdx)
	{
		for (int32 ColIdx = 0; ColIdx < SrcSize.X; ++ColIdx)
		{
			RowDst[ColIdx] = ColorValue;
		}

		RowDst += DstSize.X;
	}
}



struct FRawMeshUVTransform
{
	FVector2D Offset;
	FVector2D Scale;

	bool IsValid() const
	{
		return (Scale != FVector2D::ZeroVector);
	}
};

static FVector2D GetValidUV(const FVector2D& UV)
{
	FVector2D NewUV = UV;
	// first make sure they're positive
	if (UV.X < 0.0f)
	{
		NewUV.X = UV.X + FMath::CeilToInt(FMath::Abs(UV.X));
	}

	if (UV.Y < 0.0f)
	{
		NewUV.Y = UV.Y + FMath::CeilToInt(FMath::Abs(UV.Y));
	}

	// now make sure they're within [0, 1]
	if (UV.X > 1.0f)
	{
		NewUV.X = FMath::Fmod(NewUV.X, 1.0f);
	}

	if (UV.Y > 1.0f)
	{
		NewUV.Y = FMath::Fmod(NewUV.Y, 1.0f);
	}

	return NewUV;
}

static void MergeMaterials(UWorld* InWorld, const TArray<UMaterialInterface*>& InMaterialList, FFlattenMaterial& OutMergedMaterial, TArray<FRawMeshUVTransform>& OutUVTransforms)
{
	OutUVTransforms.Reserve(InMaterialList.Num());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// add log to warn which material it
	UE_LOG(LogMeshUtilities, Log, TEXT("Merging Material: Merge Material count %d"), InMaterialList.Num());

	int32 Index = 0;
	for (auto& MaterialIter : InMaterialList)
	{
		if (MaterialIter)
		{
			UE_LOG(LogMeshUtilities, Log, TEXT("Material List %d - %s"), ++Index, *MaterialIter->GetName());
		}
		else
		{
			UE_LOG(LogMeshUtilities, Log, TEXT("Material List %d - null material"), ++Index);
		}
	}

#endif
	// We support merging only for opaque materials
	int32 NumOpaqueMaterials = 0;
	// Fill output UV transforms with invalid values
	for (auto Material : InMaterialList)
	{
		if (Material->GetBlendMode() == BLEND_Opaque)
		{
			NumOpaqueMaterials++;
		}

		// Invalid UV transform
		FRawMeshUVTransform UVTransform;
		UVTransform.Offset = FVector2D::ZeroVector;
		UVTransform.Scale = FVector2D::ZeroVector;
		OutUVTransforms.Add(UVTransform);
	}

	if (NumOpaqueMaterials == 0)
	{
		// Nothing to merge
		return;
	}

	int32 AtlasGridSize = FMath::CeilToInt(FMath::Sqrt(NumOpaqueMaterials));
	FIntPoint AtlasTextureSize = OutMergedMaterial.DiffuseSize;
	FIntPoint ExportTextureSize = AtlasTextureSize / AtlasGridSize;
	int32 AtlasNumSamples = AtlasTextureSize.X*AtlasTextureSize.Y;

	bool bExportNormal = (OutMergedMaterial.NormalSize != FIntPoint::ZeroValue);
	bool bExportMetallic = (OutMergedMaterial.MetallicSize != FIntPoint::ZeroValue);
	bool bExportRoughness = (OutMergedMaterial.RoughnessSize != FIntPoint::ZeroValue);
	bool bExportSpecular = (OutMergedMaterial.SpecularSize != FIntPoint::ZeroValue);

	// Pre-allocate buffers for texture atlas
	OutMergedMaterial.DiffuseSamples.Reserve(AtlasNumSamples);
	OutMergedMaterial.DiffuseSamples.SetNumZeroed(AtlasNumSamples);
	if (bExportNormal)
	{
		check(OutMergedMaterial.NormalSize == OutMergedMaterial.DiffuseSize);
		OutMergedMaterial.NormalSamples.Reserve(AtlasNumSamples);
		OutMergedMaterial.NormalSamples.SetNumZeroed(AtlasNumSamples);
	}
	if (bExportMetallic)
	{
		check(OutMergedMaterial.MetallicSize == OutMergedMaterial.DiffuseSize);
		OutMergedMaterial.MetallicSamples.Reserve(AtlasNumSamples);
		OutMergedMaterial.MetallicSamples.SetNumZeroed(AtlasNumSamples);
	}
	if (bExportRoughness)
	{
		check(OutMergedMaterial.RoughnessSize == OutMergedMaterial.DiffuseSize);
		OutMergedMaterial.RoughnessSamples.Reserve(AtlasNumSamples);
		OutMergedMaterial.RoughnessSamples.SetNumZeroed(AtlasNumSamples);
	}
	if (bExportSpecular)
	{
		check(OutMergedMaterial.SpecularSize == OutMergedMaterial.DiffuseSize);
		OutMergedMaterial.SpecularSamples.Reserve(AtlasNumSamples);
		OutMergedMaterial.SpecularSamples.SetNumZeroed(AtlasNumSamples);
	}

	int32 AtlasRowIdx = 0;
	int32 AtlasColIdx = 0;
	FIntPoint AtlasTargetPos = FIntPoint(0, 0);

	// Flatten all materials and merge them into one material using texture atlases
	for (int32 MatIdx = 0; MatIdx < InMaterialList.Num(); ++MatIdx)
	{
		UMaterialInterface* Material = InMaterialList[MatIdx];
		if (Material->GetBlendMode() != BLEND_Opaque)
		{
			continue;
		}

		FFlattenMaterial FlatMaterial;
		FlatMaterial.DiffuseSize = ExportTextureSize;
		FlatMaterial.NormalSize = bExportNormal ? ExportTextureSize : FIntPoint::ZeroValue;
		FlatMaterial.MetallicSize = bExportMetallic ? ExportTextureSize : FIntPoint::ZeroValue;
		FlatMaterial.RoughnessSize = bExportRoughness ? ExportTextureSize : FIntPoint::ZeroValue;
		FlatMaterial.SpecularSize = bExportSpecular ? ExportTextureSize : FIntPoint::ZeroValue;
		int32 ExportNumSamples = ExportTextureSize.X*ExportTextureSize.Y;

		FMaterialUtilities::ExportMaterial(Material, FlatMaterial);

		if (FlatMaterial.DiffuseSamples.Num() == ExportNumSamples)
		{
			CopyTextureRect(FlatMaterial.DiffuseSamples.GetData(), ExportTextureSize, OutMergedMaterial.DiffuseSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.DiffuseSamples.Num() == 1)
		{
			SetTextureRect(FlatMaterial.DiffuseSamples[0], ExportTextureSize, OutMergedMaterial.DiffuseSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}

		if (FlatMaterial.NormalSamples.Num() == ExportNumSamples)
		{
			CopyTextureRect(FlatMaterial.NormalSamples.GetData(), ExportTextureSize, OutMergedMaterial.NormalSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.NormalSamples.Num() == 1)
		{
			SetTextureRect(FlatMaterial.NormalSamples[0], ExportTextureSize, OutMergedMaterial.NormalSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}

		if (FlatMaterial.MetallicSamples.Num() == ExportNumSamples)
		{
			CopyTextureRect(FlatMaterial.MetallicSamples.GetData(), ExportTextureSize, OutMergedMaterial.MetallicSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.MetallicSamples.Num() == 1)
		{
			SetTextureRect(FlatMaterial.MetallicSamples[0], ExportTextureSize, OutMergedMaterial.MetallicSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}

		if (FlatMaterial.RoughnessSamples.Num() == ExportNumSamples)
		{
			CopyTextureRect(FlatMaterial.RoughnessSamples.GetData(), ExportTextureSize, OutMergedMaterial.RoughnessSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.RoughnessSamples.Num() == 1)
		{
			SetTextureRect(FlatMaterial.RoughnessSamples[0], ExportTextureSize, OutMergedMaterial.RoughnessSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}

		if (FlatMaterial.SpecularSamples.Num() == ExportNumSamples)
		{
			CopyTextureRect(FlatMaterial.SpecularSamples.GetData(), ExportTextureSize, OutMergedMaterial.SpecularSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.SpecularSamples.Num() == 1)
		{
			SetTextureRect(FlatMaterial.SpecularSamples[0], ExportTextureSize, OutMergedMaterial.SpecularSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}

		check(OutUVTransforms.IsValidIndex(MatIdx));

		OutUVTransforms[MatIdx].Offset = FVector2D(
			(float)AtlasTargetPos.X / AtlasTextureSize.X,
			(float)AtlasTargetPos.Y / AtlasTextureSize.Y);

		OutUVTransforms[MatIdx].Scale = FVector2D(
			(float)ExportTextureSize.X / AtlasTextureSize.X,
			(float)ExportTextureSize.Y / AtlasTextureSize.Y);

		AtlasColIdx++;
		if (AtlasColIdx >= AtlasGridSize)
		{
			AtlasColIdx = 0;
			AtlasRowIdx++;
		}

		AtlasTargetPos = FIntPoint(AtlasColIdx*ExportTextureSize.X, AtlasRowIdx*ExportTextureSize.Y);
	}
}


static void MergeFlattenedMaterials(TArray<struct FFlattenMaterial>& InMaterialList, FFlattenMaterial& OutMergedMaterial, TArray<FRawMeshUVTransform>& OutUVTransforms)
{
	OutUVTransforms.Reserve(InMaterialList.Num());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	//// add log to warn which material it
	//UE_LOG(LogMeshUtilities, Log, TEXT("Merging Material: Merge Material count %d"), InMaterialList.Num());

	//int32 Index = 0;
	//for (auto& MaterialIter : InMaterialList)
	//{
	//	if (MaterialIter)
	//	{
	//		UE_LOG(LogMeshUtilities, Log, TEXT("Material List %d - %s"), ++Index, *MaterialIter->GetName());
	//	}
	//	else
	//	{
	//		UE_LOG(LogMeshUtilities, Log, TEXT("Material List %d - null material"), ++Index);
	//	}
	//}

#endif
	// We support merging only for opaque materials
	// Fill output UV transforms with invalid values
	for (auto Material : InMaterialList)
	{
		// Invalid UV transform
		FRawMeshUVTransform UVTransform;
		UVTransform.Offset = FVector2D::ZeroVector;
		UVTransform.Scale = FVector2D::ZeroVector;
		OutUVTransforms.Add(UVTransform);
	}

	int32 AtlasGridSize = FMath::CeilToInt(FMath::Sqrt(InMaterialList.Num()));
	FIntPoint AtlasTextureSize = OutMergedMaterial.DiffuseSize;
	FIntPoint ExportTextureSize = AtlasTextureSize / AtlasGridSize;
	int32 AtlasNumSamples = AtlasTextureSize.X*AtlasTextureSize.Y;

	bool bExportNormal = (OutMergedMaterial.NormalSize != FIntPoint::ZeroValue);
	bool bExportMetallic = (OutMergedMaterial.MetallicSize != FIntPoint::ZeroValue);
	bool bExportRoughness = (OutMergedMaterial.RoughnessSize != FIntPoint::ZeroValue);
	bool bExportSpecular = (OutMergedMaterial.SpecularSize != FIntPoint::ZeroValue);
	bool bExportEmissive = (OutMergedMaterial.EmissiveSize != FIntPoint::ZeroValue);
	bool bExportOpacity = (OutMergedMaterial.OpacitySize != FIntPoint::ZeroValue);

	// Pre-allocate buffers for texture atlas
	OutMergedMaterial.DiffuseSamples.Reserve(AtlasNumSamples);
	OutMergedMaterial.DiffuseSamples.SetNumZeroed(AtlasNumSamples);
	if (bExportNormal)
	{
		check(OutMergedMaterial.NormalSize == OutMergedMaterial.DiffuseSize);
		OutMergedMaterial.NormalSamples.Reserve(AtlasNumSamples);
		OutMergedMaterial.NormalSamples.SetNumZeroed(AtlasNumSamples);
	}
	if (bExportMetallic)
	{
		check(OutMergedMaterial.MetallicSize == OutMergedMaterial.DiffuseSize);
		OutMergedMaterial.MetallicSamples.Reserve(AtlasNumSamples);
		OutMergedMaterial.MetallicSamples.SetNumZeroed(AtlasNumSamples);
	}
	if (bExportRoughness)
	{
		check(OutMergedMaterial.RoughnessSize == OutMergedMaterial.DiffuseSize);
		OutMergedMaterial.RoughnessSamples.Reserve(AtlasNumSamples);
		OutMergedMaterial.RoughnessSamples.SetNumZeroed(AtlasNumSamples);
	}
	if (bExportSpecular)
	{
		check(OutMergedMaterial.SpecularSize == OutMergedMaterial.DiffuseSize);
		OutMergedMaterial.SpecularSamples.Reserve(AtlasNumSamples);
		OutMergedMaterial.SpecularSamples.SetNumZeroed(AtlasNumSamples);
	}
	if (bExportEmissive)
	{
		check(OutMergedMaterial.EmissiveSize == OutMergedMaterial.EmissiveSize);
		OutMergedMaterial.EmissiveSamples.Reserve(AtlasNumSamples);
		OutMergedMaterial.EmissiveSamples.SetNumZeroed(AtlasNumSamples);
	}

	if (bExportOpacity)
	{
		check(OutMergedMaterial.OpacitySize == OutMergedMaterial.OpacitySize);
		OutMergedMaterial.OpacitySamples.Reserve(AtlasNumSamples);
		OutMergedMaterial.OpacitySamples.SetNumZeroed(AtlasNumSamples);
	}


	int32 AtlasRowIdx = 0;
	int32 AtlasColIdx = 0;
	FIntPoint AtlasTargetPos = FIntPoint(0, 0);

	// Flatten all materials and merge them into one material using texture atlases
	for (int32 MatIdx = 0; MatIdx < InMaterialList.Num(); ++MatIdx)
	{
		FFlattenMaterial& FlatMaterial = InMaterialList[MatIdx];

		if (FlatMaterial.DiffuseSamples.Num() >= 1)
		{
			FlatMaterial.DiffuseSize = ConditionalImageResize(FlatMaterial.DiffuseSize, ExportTextureSize, FlatMaterial.DiffuseSamples, false);
			CopyTextureRect(FlatMaterial.DiffuseSamples.GetData(), ExportTextureSize, OutMergedMaterial.DiffuseSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.DiffuseSamples.Num() == 1)
		{
			SetTextureRect(FlatMaterial.DiffuseSamples[0], ExportTextureSize, OutMergedMaterial.DiffuseSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}

		if (FlatMaterial.NormalSamples.Num() >= 1 && bExportNormal)
		{
			FlatMaterial.NormalSize = ConditionalImageResize(FlatMaterial.NormalSize, ExportTextureSize, FlatMaterial.NormalSamples, false);
			CopyTextureRect(FlatMaterial.NormalSamples.GetData(), ExportTextureSize, OutMergedMaterial.NormalSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.NormalSamples.Num() == 1 && bExportNormal)
		{
			SetTextureRect(FlatMaterial.NormalSamples[0], ExportTextureSize, OutMergedMaterial.NormalSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}

		if (FlatMaterial.MetallicSamples.Num() >= 1 && bExportMetallic)
		{
			FlatMaterial.MetallicSize = ConditionalImageResize(FlatMaterial.MetallicSize, ExportTextureSize, FlatMaterial.MetallicSamples, false);
			CopyTextureRect(FlatMaterial.MetallicSamples.GetData(), ExportTextureSize, OutMergedMaterial.MetallicSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.MetallicSamples.Num() == 1 && bExportMetallic)
		{
			SetTextureRect(FlatMaterial.MetallicSamples[0], ExportTextureSize, OutMergedMaterial.MetallicSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}

		if (FlatMaterial.RoughnessSamples.Num() >= 1 && bExportRoughness)
		{
			FlatMaterial.RoughnessSize = ConditionalImageResize(FlatMaterial.RoughnessSize, ExportTextureSize, FlatMaterial.RoughnessSamples, false);
			CopyTextureRect(FlatMaterial.RoughnessSamples.GetData(), ExportTextureSize, OutMergedMaterial.RoughnessSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.RoughnessSamples.Num() == 1 && bExportRoughness)
		{
			SetTextureRect(FlatMaterial.RoughnessSamples[0], ExportTextureSize, OutMergedMaterial.RoughnessSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}

		if (FlatMaterial.SpecularSamples.Num() >= 1 && bExportSpecular)
		{
			FlatMaterial.SpecularSize = ConditionalImageResize(FlatMaterial.SpecularSize, ExportTextureSize, FlatMaterial.SpecularSamples, false);
			CopyTextureRect(FlatMaterial.SpecularSamples.GetData(), ExportTextureSize, OutMergedMaterial.SpecularSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.SpecularSamples.Num() == 1 && bExportSpecular)
		{
			SetTextureRect(FlatMaterial.SpecularSamples[0], ExportTextureSize, OutMergedMaterial.SpecularSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}

		if (FlatMaterial.EmissiveSamples.Num() >= 1)
		{
			FlatMaterial.SpecularSize = bExportEmissive ? ConditionalImageResize(FlatMaterial.EmissiveSize, ExportTextureSize, FlatMaterial.EmissiveSamples, false) : FIntPoint::ZeroValue;
			CopyTextureRect(FlatMaterial.EmissiveSamples.GetData(), ExportTextureSize, OutMergedMaterial.EmissiveSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.EmissiveSamples.Num() == 1)
		{
			SetTextureRect(FlatMaterial.EmissiveSamples[0], ExportTextureSize, OutMergedMaterial.EmissiveSamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}

		if (FlatMaterial.OpacitySamples.Num() >= 1)
		{
			FlatMaterial.SpecularSize = bExportOpacity ? ConditionalImageResize(FlatMaterial.OpacitySize, ExportTextureSize, FlatMaterial.OpacitySamples, false) : FIntPoint::ZeroValue;
			CopyTextureRect(FlatMaterial.OpacitySamples.GetData(), ExportTextureSize, OutMergedMaterial.OpacitySamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}
		else if (FlatMaterial.OpacitySamples.Num() == 1)
		{
			SetTextureRect(FlatMaterial.OpacitySamples[0], ExportTextureSize, OutMergedMaterial.OpacitySamples.GetData(), AtlasTextureSize, AtlasTargetPos);
		}


		check(OutUVTransforms.IsValidIndex(MatIdx));

		OutUVTransforms[MatIdx].Offset = FVector2D(
			(float)AtlasTargetPos.X / AtlasTextureSize.X,
			(float)AtlasTargetPos.Y / AtlasTextureSize.Y);

		OutUVTransforms[MatIdx].Scale = FVector2D(
			(float)ExportTextureSize.X / AtlasTextureSize.X,
			(float)ExportTextureSize.Y / AtlasTextureSize.Y);

		AtlasColIdx++;
		if (AtlasColIdx >= AtlasGridSize)
		{
			AtlasColIdx = 0;
			AtlasRowIdx++;
		}

		AtlasTargetPos = FIntPoint(AtlasColIdx*ExportTextureSize.X, AtlasRowIdx*ExportTextureSize.Y);
	}
}

void FMeshUtilities::MergeActors(
	const TArray<AActor*>& SourceActors,
	const FMeshMergingSettings& InSettings,
	UPackage* InOuter,
	const FString& InBasePackageName,
	int32 UseLOD, // does not build all LODs but only use this LOD to create base mesh
	TArray<UObject*>& OutAssetsToSync,
	FVector& OutMergedActorLocation,
	bool bSilent) const
{
	MergeActors(SourceActors, InSettings, InOuter, InBasePackageName, OutAssetsToSync, OutMergedActorLocation, bSilent);
}

void FMeshUtilities::MergeActors(
	const TArray<AActor*>& SourceActors,
	const FMeshMergingSettings& InSettings,
	UPackage* InOuter,
	const FString& InBasePackageName,
	TArray<UObject*>& OutAssetsToSync,
	FVector& OutMergedActorLocation,
	bool bSilent) const
{
	checkf(SourceActors.Num(), TEXT("No actors supplied for merging"));
	
	TArray<UStaticMeshComponent*> ComponentsToMerge;
	ComponentsToMerge.Reserve(SourceActors.Num());
	// Collect static mesh components
	for (AActor* Actor : SourceActors)
	{
		TInlineComponentArray<UStaticMeshComponent*> Components;
		Actor->GetComponents<UStaticMeshComponent>(Components);

		// Filter out bad components
		for (UStaticMeshComponent* MeshComponent : Components)
		{
			if (MeshComponent->StaticMesh != nullptr &&
				MeshComponent->StaticMesh->SourceModels.Num() > 0)
			{
				ComponentsToMerge.Add(MeshComponent);
			}
		}
	}

	checkf(SourceActors.Num(), TEXT("No valid components found in actors supplied for merging"));

	UWorld* World = SourceActors[0]->GetWorld();
	checkf(World != nullptr, TEXT("Invalid world retrieved from Actor"));
	const float ScreenAreaSize = TNumericLimits<float>::Max();
	MergeStaticMeshComponents(ComponentsToMerge, World, InSettings, InOuter, InBasePackageName, OutAssetsToSync, OutMergedActorLocation, ScreenAreaSize, bSilent);
}

static void CheckWrappingUVs(TArray<FRawMeshExt>& SourceMeshes, TArray<bool>& MeshShouldBakeVertexData)
{
	const uint32 MeshCount = SourceMeshes.Num();
	for (uint32 MeshIndex = 0; MeshIndex < MeshCount; ++MeshIndex)
	{
		FRawMeshExt& SourceMesh = SourceMeshes[MeshIndex];
		const int32 LODIndex = SourceMeshes[MeshIndex].ExportLODIndex;
		if (SourceMesh.bShouldExportLOD[LODIndex])
		{
			FRawMesh* RawMesh = SourceMesh.MeshLODData[LODIndex].RawMesh;
			check(RawMesh);

			for (uint32 ChannelIndex = 0; ChannelIndex < MAX_MESH_TEXTURE_COORDS; ++ChannelIndex)
			{
				bool bProcessed = false;
				bool bHasCoordinates = (RawMesh->WedgeTexCoords[ChannelIndex].Num() != 0);

				if (bHasCoordinates)
				{
					FVector2D Min(FLT_MAX, FLT_MAX);
					FVector2D Max(-FLT_MAX, -FLT_MAX);
					for (const FVector2D& Coordinate : RawMesh->WedgeTexCoords[ChannelIndex])
					{						
						if ((FMath::IsNegativeFloat(Coordinate.X) || FMath::IsNegativeFloat(Coordinate.Y)) || (Coordinate.X > ( 1.0f + KINDA_SMALL_NUMBER) || Coordinate.Y > (1.0f + KINDA_SMALL_NUMBER)))
						{
							MeshShouldBakeVertexData[MeshIndex] = true;
							bProcessed = true;
							break;
						}
					}
				}

				if (bProcessed)
				{
					break;
				}
			}
		}
	}
}

void FMeshUtilities::MergeStaticMeshComponents(const TArray<UStaticMeshComponent*>& ComponentsToMerge, UWorld* World, const FMeshMergingSettings& InSettings, UPackage* InOuter, const FString& InBasePackageName, TArray<UObject*>& OutAssetsToSync, FVector& OutMergedActorLocation, const float ScreenAreaSize, bool bSilent /*= false*/) const
{
	TArray<UMaterialInterface*>						UniqueMaterials;
	TMap<FMeshIdAndLOD, TArray<int32>>				MaterialMap;
	TArray<FRawMeshExt>								SourceMeshes;
	bool											bWithVertexColors[MAX_STATIC_MESH_LODS] = {};
	bool											bOcuppiedUVChannels[MAX_STATIC_MESH_LODS][MAX_MESH_TEXTURE_COORDS] = {};
	UBodySetup*										BodySetupSource = nullptr;
	
	checkf(ComponentsToMerge.Num(), TEXT("No valid components supplied for merging"));

	SourceMeshes.AddZeroed(ComponentsToMerge.Num());

	FScopedSlowTask MainTask(100, LOCTEXT("MeshUtilities_MergeStaticMeshComponents", "Merging StaticMesh Components"));
	MainTask.MakeDialog();

	// Use first mesh for naming and pivot
	FString MergedAssetPackageName;
	FVector MergedAssetPivot;

	int32 NumMaxLOD = 0;	
	for (int32 MeshId = 0; MeshId < ComponentsToMerge.Num(); ++MeshId)
	{
		UStaticMeshComponent* MeshComponent = ComponentsToMerge[MeshId];
	
		// Determine the maximum number of LOD levels found in the source meshes
		NumMaxLOD = FMath::Max(NumMaxLOD, MeshComponent->StaticMesh->SourceModels.Num());

		// Save the pivot and asset package name of the first mesh, will later be used for creating merged mesh asset 
		if (MeshId == 0)
		{
			// Mesh component pivot point
			MergedAssetPivot = InSettings.bPivotPointAtZero ? FVector::ZeroVector : MeshComponent->ComponentToWorld.GetLocation();
			// Source mesh asset package name
			MergedAssetPackageName = MeshComponent->StaticMesh->GetOutermost()->GetName();
		}
	}

	// Cap the number of LOD levels to the max
	NumMaxLOD = FMath::Min(NumMaxLOD, MAX_STATIC_MESH_LODS);

	int32 BaseLODIndex = 0;
	// Are we going to export a single LOD or not
	if (InSettings.LODSelectionType == EMeshLODSelectionType::SpecificLOD && InSettings.SpecificLOD >= 0)
	{
		// Will export only one specified LOD as LOD0 for the merged mesh
		BaseLODIndex = FMath::Max(0, FMath::Min(InSettings.SpecificLOD, MAX_STATIC_MESH_LODS));
	}

	const bool bMergeAllAvailableLODs = InSettings.LODSelectionType == EMeshLODSelectionType::AllLODs;
	MainTask.EnterProgressFrame(10, LOCTEXT("MeshUtilities_MergeStaticMeshComponents_RetrievingRawMesh", "Retrieving Raw Meshes"));
		
	for (int32 MeshId = 0; MeshId < ComponentsToMerge.Num(); ++MeshId)
	{
		UStaticMeshComponent* StaticMeshComponent = ComponentsToMerge[MeshId];

		// LOD index will be overridden if the user has chosen to pick it according to the viewing distance
		int32 CalculatedLODIndex = -1;
		if (InSettings.LODSelectionType == EMeshLODSelectionType::CalculateLOD && ScreenAreaSize > 0.0f && ScreenAreaSize < 1.0f)
		{
			FHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
			IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
			CalculatedLODIndex = Utilities->GetLODLevelForScreenAreaSize(StaticMeshComponent, ScreenAreaSize);
		}

		// Retrieve the lowest available LOD level from the mesh 
		int32 StartLODIndex = InSettings.LODSelectionType == EMeshLODSelectionType::CalculateLOD ? CalculatedLODIndex : FMath::Min(BaseLODIndex, StaticMeshComponent->StaticMesh->SourceModels.Num() - 1);
		int32 EndLODIndex = bMergeAllAvailableLODs ? FMath::Min(StaticMeshComponent->StaticMesh->SourceModels.Num(), MAX_STATIC_MESH_LODS) : StartLODIndex + 1;

		SourceMeshes[MeshId].MaxLODExport = EndLODIndex - 1;

		// Set export LOD index if we are exporting one specifically
		SourceMeshes[MeshId].ExportLODIndex = !bMergeAllAvailableLODs ? StartLODIndex : -1;

		for (int32 LODIndex = StartLODIndex; LODIndex < EndLODIndex; ++LODIndex)
		{
			// Store source static mesh and set LOD export flag
			SourceMeshes[MeshId].SourceStaticMesh = StaticMeshComponent->StaticMesh;
			SourceMeshes[MeshId].bShouldExportLOD[LODIndex] = true;

			TArray<int32> MeshMaterialMap;
			// Retrieve and construct raw mesh from source meshes
			SourceMeshes[MeshId].MeshLODData[LODIndex].RawMesh = new FRawMesh();
			FRawMesh* RawMeshLOD = SourceMeshes[MeshId].MeshLODData[LODIndex].RawMesh;
			if (ConstructRawMesh(StaticMeshComponent, LODIndex, InSettings.bBakeVertexDataToMesh || InSettings.bUseVertexDataForBakingMaterial, *RawMeshLOD, UniqueMaterials, MeshMaterialMap))
			{
				MaterialMap.Add(FMeshIdAndLOD(MeshId, LODIndex), MeshMaterialMap);

				// Check if vertex colours should be propagated
				if (InSettings.bBakeVertexDataToMesh)
				{
					// Whether at least one of the meshes has vertex colors
					bWithVertexColors[LODIndex] |= (RawMeshLOD->WedgeColors.Num() != 0);
				}

				// Which UV channels has data at least in one mesh
				for (int32 ChannelIdx = 0; ChannelIdx < MAX_MESH_TEXTURE_COORDS; ++ChannelIdx)
				{
					bOcuppiedUVChannels[LODIndex][ChannelIdx] |= (RawMeshLOD->WedgeTexCoords[ChannelIdx].Num() != 0);
				}
				if (InSettings.bUseLandscapeCulling)
				{
					// Landscape / volume culling
					CullTrianglesFromVolumesAndUnderLandscapes(StaticMeshComponent, *RawMeshLOD);
				}
			}
		}
	}
	
	if (InSettings.bMergePhysicsData)
	{
		for (int32 MeshId = 0; MeshId < ComponentsToMerge.Num(); ++MeshId)
		{
			UStaticMeshComponent* MeshComponent = ComponentsToMerge[MeshId];
			ExtractPhysicsGeometry(MeshComponent, SourceMeshes[MeshId].AggGeom);

			// We will use first valid BodySetup as a source of physics settings
			if (BodySetupSource == nullptr)
			{
				BodySetupSource = MeshComponent->StaticMesh->BodySetup;
			}
		}
	}

	MainTask.EnterProgressFrame(20);

	// Remap material indices regardless of baking out materials or not (could give a draw call decrease)
	TArray<bool> MeshShouldBakeVertexData;
	TMap<FMeshIdAndLOD, TArray<int32> > NewMaterialMap;
	TArray<UMaterialInterface*> NewStaticMeshMaterials;
	FMaterialUtilities::RemapUniqueMaterialIndices(
		UniqueMaterials,
		SourceMeshes,
		MaterialMap,
		InSettings.MaterialSettings,
		InSettings.bUseVertexDataForBakingMaterial,
		InSettings.bMergeMaterials,
		MeshShouldBakeVertexData,
		NewMaterialMap,
		NewStaticMeshMaterials);
	// Use shared material data.
	Exchange(MaterialMap, NewMaterialMap);
	Exchange(UniqueMaterials, NewStaticMeshMaterials);

	if (InSettings.bMergeMaterials && !bMergeAllAvailableLODs)
	{
		// Should merge flattened materials into one texture
		MainTask.EnterProgressFrame(20, LOCTEXT("MeshUtilities_MergeStaticMeshComponents_MergingMaterials", "Merging Materials"));
		
		// If we have UVs outside of the UV boundaries we should use unique UVs to render out the materials
		CheckWrappingUVs(SourceMeshes, MeshShouldBakeVertexData);

		// Flatten Materials
		TArray<FFlattenMaterial> FlattenedMaterials;
		FlattenMaterialsWithMeshData(UniqueMaterials, SourceMeshes, MaterialMap, MeshShouldBakeVertexData, InSettings.MaterialSettings, FlattenedMaterials);

		FIntPoint AtlasTextureSize = InSettings.MaterialSettings.TextureSize;
		FFlattenMaterial MergedFlatMaterial;
		MergedFlatMaterial.DiffuseSize = AtlasTextureSize;
		MergedFlatMaterial.NormalSize = InSettings.MaterialSettings.bNormalMap ? AtlasTextureSize : FIntPoint::ZeroValue;
		MergedFlatMaterial.MetallicSize = InSettings.MaterialSettings.bMetallicMap ? AtlasTextureSize : FIntPoint::ZeroValue;
		MergedFlatMaterial.RoughnessSize = InSettings.MaterialSettings.bRoughnessMap ? AtlasTextureSize : FIntPoint::ZeroValue;
		MergedFlatMaterial.SpecularSize = InSettings.MaterialSettings.bSpecularMap ? AtlasTextureSize : FIntPoint::ZeroValue;
		MergedFlatMaterial.EmissiveSize = InSettings.MaterialSettings.bEmissiveMap ? AtlasTextureSize : FIntPoint::ZeroValue;
		MergedFlatMaterial.OpacitySize = InSettings.MaterialSettings.bOpacityMap ? AtlasTextureSize : FIntPoint::ZeroValue;

		TArray<FRawMeshUVTransform> UVTransforms;
		MergeFlattenedMaterials(FlattenedMaterials, MergedFlatMaterial, UVTransforms);

		FMaterialUtilities::OptimizeFlattenMaterial(MergedFlatMaterial);

		// Adjust UVs and remap material indices
		for (int32 MeshIndex = 0; MeshIndex < SourceMeshes.Num(); ++MeshIndex)
		{
			const int32 LODIndex = SourceMeshes[MeshIndex].ExportLODIndex;
			FRawMesh& RawMesh = *SourceMeshes[MeshIndex].MeshLODData[LODIndex].RawMesh;
			if (RawMesh.VertexPositions.Num())
			{
				const TArray<int32> MaterialIndices = MaterialMap[FMeshIdAndLOD(MeshIndex, LODIndex)];

				// If we end up in the situation where we have two of the same meshes which require baking vertex data (thus unique UVs), the first one to be found in the array will be used to bake out the material and generate new uvs for it. The other one however will not have the new UVs and thus the baked out material does not match up with its uvs which makes the mesh be UVed incorrectly with the new baked material.
				if (!SourceMeshes[MeshIndex].MeshLODData[LODIndex].NewUVs.Num() && MeshShouldBakeVertexData[MeshIndex])
				{
					// Calculate the max bounds for this raw mesh 
					CalculateTextureCoordinateBoundsForRawMesh(*SourceMeshes[MeshIndex].MeshLODData[LODIndex].RawMesh, SourceMeshes[MeshIndex].MeshLODData[LODIndex].TexCoordBounds);

					// Generate unique UVs
					GenerateUniqueUVsForStaticMesh(*SourceMeshes[MeshIndex].MeshLODData[LODIndex].RawMesh, InSettings.MaterialSettings.TextureSize.GetMax(), SourceMeshes[MeshIndex].MeshLODData[LODIndex].NewUVs);
				}

				for (int32 UVChannelIdx = 0; UVChannelIdx < MAX_MESH_TEXTURE_COORDS; ++UVChannelIdx)
				{
					// Determine if we should use original or non-overlapping generated UVs
					TArray<FVector2D>& UVs = SourceMeshes[MeshIndex].MeshLODData[LODIndex].NewUVs.Num() ? SourceMeshes[MeshIndex].MeshLODData[LODIndex].NewUVs : RawMesh.WedgeTexCoords[UVChannelIdx];
					if (RawMesh.WedgeTexCoords[UVChannelIdx].Num() > 0)
					{
						int32 UVIdx = 0;
						for (int32 FaceMaterialIndex : RawMesh.FaceMaterialIndices)
						{
							const FRawMeshUVTransform& UVTransform = UVTransforms[MaterialIndices[FaceMaterialIndex]];
							if (UVTransform.IsValid())
							{
								FVector2D UV0 = GetValidUV(UVs[UVIdx + 0]);
								FVector2D UV1 = GetValidUV(UVs[UVIdx + 1]);
								FVector2D UV2 = GetValidUV(UVs[UVIdx + 2]);
								RawMesh.WedgeTexCoords[UVChannelIdx][UVIdx + 0] = UV0 * UVTransform.Scale + UVTransform.Offset;
								RawMesh.WedgeTexCoords[UVChannelIdx][UVIdx + 1] = UV1 * UVTransform.Scale + UVTransform.Offset;
								RawMesh.WedgeTexCoords[UVChannelIdx][UVIdx + 2] = UV2 * UVTransform.Scale + UVTransform.Offset;
							}

							UVIdx += 3;
						}
					} 
				}

				// Reset material indexes
				for (int32& FaceMaterialIndex : RawMesh.FaceMaterialIndices)
				{
					FaceMaterialIndex = 0;
				}
			}
			else
			{
				break;
			}
		}

		// Create merged material asset
		FString MaterialAssetName;
		FString MaterialPackageName;
		if (InBasePackageName.IsEmpty())
		{
			MaterialAssetName = TEXT("M_MERGED_") + FPackageName::GetShortName(MergedAssetPackageName);
			MaterialPackageName = FPackageName::GetLongPackagePath(MergedAssetPackageName) + TEXT("/") + MaterialAssetName;
		}
		else
		{
			MaterialAssetName = TEXT("M_") + FPackageName::GetShortName(InBasePackageName);
			MaterialPackageName = FPackageName::GetLongPackagePath(InBasePackageName) + TEXT("/") + MaterialAssetName;
		}

		UPackage* MaterialPackage = InOuter;
		if (MaterialPackage == nullptr)
		{
			MaterialPackage = CreatePackage(nullptr, *MaterialPackageName);
			check(MaterialPackage);
			MaterialPackage->FullyLoad();
			MaterialPackage->Modify();
		}

		UMaterialInstanceConstant* MergedMaterial = ProxyMaterialUtilities::CreateProxyMaterialInstance(MaterialPackage, InSettings.MaterialSettings, MergedFlatMaterial, MaterialAssetName, MaterialPackageName);
		// Set material static lighting usage flag if project has static lighting enabled
		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnGameThread() != 0);
		if (bAllowStaticLighting)
		{
			MergedMaterial->CheckMaterialUsage(MATUSAGE_StaticLighting);
		}

		// Only end up with one material so clear array first
		UniqueMaterials.Empty();
		UniqueMaterials.Add(MergedMaterial);
	}

	MainTask.EnterProgressFrame(20, LOCTEXT("MeshUtilities_MergeStaticMeshComponents_MergingMeshes", "Merging Meshes"));

	FRawMeshExt MergedMesh;
	FMemory::Memset(&MergedMesh, 0, sizeof(MergedMesh));

	int32 MaxExportLODs = bMergeAllAvailableLODs ? NumMaxLOD : 1;
	// Merge meshes into single mesh
	for (int32 SourceMeshIdx = 0; SourceMeshIdx < SourceMeshes.Num(); ++SourceMeshIdx)
	{	
		for (int32 TargetLODIndex = 0; TargetLODIndex < MaxExportLODs; ++TargetLODIndex)
		{
			int32 SourceLODIndex = SourceMeshes[SourceMeshIdx].bShouldExportLOD[TargetLODIndex] ? TargetLODIndex : (SourceMeshes[SourceMeshIdx].MaxLODExport);

			if (!bMergeAllAvailableLODs)
			{
				SourceLODIndex = SourceMeshes[SourceMeshIdx].ExportLODIndex;
			}
			
			// Allocate raw meshes where needed 
			if (MergedMesh.MeshLODData[TargetLODIndex].RawMesh == nullptr)
			{
				MergedMesh.MeshLODData[TargetLODIndex].RawMesh = new FRawMesh();
			}

			// Merge vertex data from source mesh list into single mesh
			const FRawMesh& SourceRawMesh = *SourceMeshes[SourceMeshIdx].MeshLODData[SourceLODIndex].RawMesh;

			if (SourceRawMesh.VertexPositions.Num() == 0)
			{
				continue;
			}

			const TArray<int32> MaterialIndices = MaterialMap[FMeshIdAndLOD(SourceMeshIdx, SourceLODIndex)];
			check(MaterialIndices.Num() > 0);

			FRawMesh& TargetRawMesh = *MergedMesh.MeshLODData[TargetLODIndex].RawMesh;
			TargetRawMesh.FaceSmoothingMasks.Append(SourceRawMesh.FaceSmoothingMasks);

			if (InSettings.bMergeMaterials && !bMergeAllAvailableLODs)
			{
				TargetRawMesh.FaceMaterialIndices.AddZeroed(SourceRawMesh.FaceMaterialIndices.Num());
			}
			else
			{
				for (const int32 Index : SourceRawMesh.FaceMaterialIndices)
				{
					TargetRawMesh.FaceMaterialIndices.Add(MaterialIndices[Index]);
				}
			}

			int32 IndicesOffset = TargetRawMesh.VertexPositions.Num();

			for (int32 Index : SourceRawMesh.WedgeIndices)
			{
				TargetRawMesh.WedgeIndices.Add(Index + IndicesOffset);
			}

			for (FVector VertexPos : SourceRawMesh.VertexPositions)
			{
				TargetRawMesh.VertexPositions.Add(VertexPos - MergedAssetPivot);
			}

			TargetRawMesh.WedgeTangentX.Append(SourceRawMesh.WedgeTangentX);
			TargetRawMesh.WedgeTangentY.Append(SourceRawMesh.WedgeTangentY);
			TargetRawMesh.WedgeTangentZ.Append(SourceRawMesh.WedgeTangentZ);

			// Deal with vertex colors
			// Some meshes may have it, in this case merged mesh will be forced to have vertex colors as well
			if (bWithVertexColors[SourceLODIndex] && InSettings.bBakeVertexDataToMesh)
			{
				if (SourceRawMesh.WedgeColors.Num())
				{
					TargetRawMesh.WedgeColors.Append(SourceRawMesh.WedgeColors);
				}
				else
				{
					// In case this source mesh does not have vertex colors, fill target with 0xFF
					int32 ColorsOffset = TargetRawMesh.WedgeColors.Num();
					int32 ColorsNum = SourceRawMesh.WedgeIndices.Num();
					TargetRawMesh.WedgeColors.AddUninitialized(ColorsNum);
					FMemory::Memset(&TargetRawMesh.WedgeColors[ColorsOffset], 0xFF, ColorsNum*TargetRawMesh.WedgeColors.GetTypeSize());
				}
			}

			// Merge all other UV channels 
			for (int32 ChannelIdx = 0; ChannelIdx < MAX_MESH_TEXTURE_COORDS; ++ChannelIdx)
			{
				// Whether this channel has data
				if (bOcuppiedUVChannels[SourceLODIndex][ChannelIdx])
				{
					const TArray<FVector2D>& SourceChannel = SourceRawMesh.WedgeTexCoords[ChannelIdx];
					TArray<FVector2D>& TargetChannel = TargetRawMesh.WedgeTexCoords[ChannelIdx];

					// Whether source mesh has data in this channel
					if (SourceChannel.Num())
					{
						TargetChannel.Append(SourceChannel);
					}
					else
					{
						// Fill with zero coordinates if source mesh has no data for this channel
						const int32 TexCoordNum = SourceRawMesh.WedgeIndices.Num();
						for (int32 CoordIdx = 0; CoordIdx < TexCoordNum; ++CoordIdx)
						{
							TargetChannel.Add(FVector2D::ZeroVector);
						}
					}
				}
			}			
		}
	}

	// Transform physics primitives to merged mesh pivot
	if (InSettings.bMergePhysicsData && !MergedAssetPivot.IsZero())
	{
		FTransform PivotTM(-MergedAssetPivot);
		for (auto& SourceMesh : SourceMeshes)
		{
			TransformPhysicsGeometry(PivotTM, SourceMesh.AggGeom);
		}
	}
	
	// Compute target lightmap channel for each LOD
	// User can specify any index, but there are should not be empty gaps in UV channel list
	int32 TargetLightMapUVChannel[MAX_STATIC_MESH_LODS];
	for (int32 LODIndex = 0; LODIndex < MAX_STATIC_MESH_LODS; ++LODIndex)
	{		
		for (int32 ChannelIdx = 0; ChannelIdx < MAX_MESH_TEXTURE_COORDS && bOcuppiedUVChannels[LODIndex][ChannelIdx]; ++ChannelIdx)
		{
			TargetLightMapUVChannel[LODIndex] = FMath::Min(InSettings.TargetLightMapUVChannel, ChannelIdx);
		}
	}

	MainTask.EnterProgressFrame(20, LOCTEXT("MeshUtilities_MergeStaticMeshComponents_CreatingMergedMeshAsset", "Creating Merged Mesh Asset"));

	//
	//Create merged mesh asset
	//
	{
		FString AssetName;
		FString PackageName;
		if (InBasePackageName.IsEmpty())
		{
			AssetName = TEXT("SM_MERGED_") + FPackageName::GetShortName(MergedAssetPackageName);
			PackageName = FPackageName::GetLongPackagePath(MergedAssetPackageName) + TEXT("/") + AssetName;
		}
		else
		{
			AssetName = FPackageName::GetShortName(InBasePackageName);
			PackageName = InBasePackageName;
		}

		UPackage* Package = InOuter;
		if (Package == nullptr)
		{
			Package = CreatePackage(NULL, *PackageName);
			check(Package);
			Package->FullyLoad();
			Package->Modify();
		}

		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);
		StaticMesh->InitResources();

		FString OutputPath = StaticMesh->GetPathName();

		// make sure it has a new lighting guid
		StaticMesh->LightingGuid = FGuid::NewGuid();
		if (InSettings.bGenerateLightMapUV)
		{
			StaticMesh->LightMapResolution = InSettings.TargetLightMapResolution;

			int32 TargetLightMapIndex = InSettings.TargetLightMapUVChannel;	
			for (int32 LODIndex = 0; LODIndex < MAX_STATIC_MESH_LODS; ++LODIndex)
			{
				for (int32 ChannelIdx = 0; ChannelIdx < MAX_MESH_TEXTURE_COORDS && bOcuppiedUVChannels[LODIndex][ChannelIdx]; ++ChannelIdx)
				{
					TargetLightMapIndex = FMath::Max(TargetLightMapIndex, ChannelIdx);
				}
			}

			StaticMesh->LightMapCoordinateIndex = TargetLightMapIndex + 1;
		}

		for (int32 LODIndex = 0; LODIndex < NumMaxLOD; ++LODIndex)
		{
			if (MergedMesh.MeshLODData[LODIndex].RawMesh != nullptr)
			{
				FRawMesh& MergedMeshLOD = *MergedMesh.MeshLODData[LODIndex].RawMesh;
				if (MergedMeshLOD.VertexPositions.Num() > 0)
				{
					FStaticMeshSourceModel* SrcModel = new (StaticMesh->SourceModels) FStaticMeshSourceModel();
					/*Don't allow the engine to recalculate normals*/
					SrcModel->BuildSettings.bRecomputeNormals = false;
					SrcModel->BuildSettings.bRecomputeTangents = false;
					SrcModel->BuildSettings.bRemoveDegenerates = false;
					SrcModel->BuildSettings.bUseHighPrecisionTangentBasis = false;
					SrcModel->BuildSettings.bUseFullPrecisionUVs = false;
					SrcModel->BuildSettings.bGenerateLightmapUVs = InSettings.bGenerateLightMapUV;
					SrcModel->BuildSettings.MinLightmapResolution = InSettings.TargetLightMapResolution;
					SrcModel->BuildSettings.SrcLightmapIndex = 0;
					SrcModel->BuildSettings.DstLightmapIndex = StaticMesh->LightMapCoordinateIndex;

					SrcModel->RawMeshBulkData->SaveRawMesh(MergedMeshLOD);
				}
			}
			
		}

		// Assign materials
		for (UMaterialInterface* Material : UniqueMaterials)
		{
			if (Material && !Material->IsAsset())
			{
				Material = nullptr; // do not save non-asset materials
			}

			StaticMesh->Materials.Add(Material);
		}

		if (InSettings.bMergePhysicsData)
		{
			StaticMesh->CreateBodySetup();
			if (BodySetupSource)
			{
				StaticMesh->BodySetup->CopyBodyPropertiesFrom(BodySetupSource);
			}

			StaticMesh->BodySetup->AggGeom = FKAggregateGeom();
			for (const FRawMeshExt& SourceMesh : SourceMeshes)
			{
				StaticMesh->BodySetup->AddCollisionFrom(SourceMesh.AggGeom);
				// Copy section/collision info from first LOD level in source static mesh
				if (SourceMesh.SourceStaticMesh)
				{
					StaticMesh->SectionInfoMap.CopyFrom(SourceMesh.SourceStaticMesh->SectionInfoMap);
				}
			}
		}

		MainTask.EnterProgressFrame(10, LOCTEXT("MeshUtilities_MergeStaticMeshComponents_BuildingStaticMesh", "Building Static Mesh"));

		StaticMesh->Build(bSilent);
		StaticMesh->PostEditChange();

		OutAssetsToSync.Add(StaticMesh);
		OutMergedActorLocation = MergedAssetPivot;
	}

	for (FRawMeshExt& SourceMesh : SourceMeshes)
	{
		for (FMeshMergeData& Mergedata : SourceMesh.MeshLODData)
		{
			Mergedata.ReleaseData();
		}
	}

	for (FMeshMergeData& Mergedata : MergedMesh.MeshLODData)
	{
		Mergedata.ReleaseData();
	}
}

void FMeshUtilities::MergeStaticMeshComponents(const TArray<UStaticMeshComponent*>& ComponentsToMerge, UWorld* World, const FMeshMergingSettings& InSettings, UPackage* InOuter, const FString& InBasePackageName, int32 UseLOD, /* does not build all LODs but only use this LOD to create base mesh */ TArray<UObject*>& OutAssetsToSync, FVector& OutMergedActorLocation, const float ScreenAreaSize, bool bSilent /*= false*/) const
{
	MergeStaticMeshComponents(ComponentsToMerge, World, InSettings, InOuter, InBasePackageName, OutAssetsToSync, OutMergedActorLocation, ScreenAreaSize, bSilent);
}

bool FMeshUtilities::RemoveBonesFromMesh(USkeletalMesh* SkeletalMesh, int32 LODIndex, const TArray<FName>* BoneNamesToRemove) const
{
	IMeshBoneReductionModule& MeshBoneReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshBoneReductionModule>("MeshBoneReduction");
	IMeshBoneReduction * MeshBoneReductionInterface = MeshBoneReductionModule.GetMeshBoneReductionInterface();

	return MeshBoneReductionInterface->ReduceBoneCounts(SkeletalMesh, LODIndex, BoneNamesToRemove);
}

/*------------------------------------------------------------------------------
Mesh reduction.
------------------------------------------------------------------------------*/

IMeshReduction* FMeshUtilities::GetMeshReductionInterface()
{
	return MeshReduction;
}

/*------------------------------------------------------------------------------
Mesh merging.
------------------------------------------------------------------------------*/
IMeshMerging* FMeshUtilities::GetMeshMergingInterface()
{
	return MeshMerging;
}

/*------------------------------------------------------------------------------
Module initialization / teardown.
------------------------------------------------------------------------------*/

void FMeshUtilities::StartupModule()
{
	check(MeshReduction == NULL);
	check(MeshMerging == NULL);

	Processor = new FProxyGenerationProcessor();

	// Look for a mesh reduction module.
	{
		TArray<FName> ModuleNames;
		FModuleManager::Get().FindModules(TEXT("*MeshReduction"), ModuleNames);
		TArray<FName> SwarmModuleNames;
		FModuleManager::Get().FindModules(TEXT("*SimplygonSwarm"), SwarmModuleNames);


		for (int32 Index = 0; Index < ModuleNames.Num(); Index++)
		{
			IMeshReductionModule& MeshReductionModule = FModuleManager::LoadModuleChecked<IMeshReductionModule>(ModuleNames[Index]);

			// Look for MeshReduction interface
			if (MeshReduction == NULL)
			{
				MeshReduction = MeshReductionModule.GetMeshReductionInterface();
				if (MeshReduction)
				{
					UE_LOG(LogMeshUtilities, Log, TEXT("Using %s for automatic mesh reduction"), *ModuleNames[Index].ToString());
				}
			}

			// Look for MeshMerging interface
			if (MeshMerging == NULL)
			{
				MeshMerging = MeshReductionModule.GetMeshMergingInterface();
				if (MeshMerging)
				{
					UE_LOG(LogMeshUtilities, Log, TEXT("Using %s for automatic mesh merging"), *ModuleNames[Index].ToString());
				}
			}

			// Break early if both interfaces were found
			if (MeshReduction && MeshMerging)
			{
				break;
			}
		}


		for (int32 Index = 0; Index < SwarmModuleNames.Num(); Index++)
		{
			IMeshReductionModule& MeshReductionModule = FModuleManager::LoadModuleChecked<IMeshReductionModule>(SwarmModuleNames[Index]);

			// Look for distributed mesh merging interface
			if (DistributedMeshMerging == NULL)
			{
				DistributedMeshMerging = MeshReductionModule.GetMeshMergingInterface();

				if (DistributedMeshMerging)
				{
					UE_LOG(LogMeshUtilities, Log, TEXT("Using %s for distributed automatic mesh merging"), *SwarmModuleNames[Index].ToString());
				}
			}
		}

		if (!MeshReduction)
		{
			UE_LOG(LogMeshUtilities, Log, TEXT("No automatic mesh reduction module available"));
		}

		if (!MeshMerging)
		{
			UE_LOG(LogMeshUtilities, Log, TEXT("No automatic mesh merging module available"));
		}
		else
		{
			MeshMerging->CompleteDelegate.BindRaw(Processor, &FProxyGenerationProcessor::ProxyGenerationComplete);
			MeshMerging->FailedDelegate.BindRaw(Processor, &FProxyGenerationProcessor::ProxyGenerationFailed);
		}

		if (!DistributedMeshMerging)
		{
			UE_LOG(LogMeshUtilities, Log, TEXT("No distributed automatic mesh merging module available"));
		}
		else
		{
			DistributedMeshMerging->CompleteDelegate.BindRaw(Processor, &FProxyGenerationProcessor::ProxyGenerationComplete);
			DistributedMeshMerging->FailedDelegate.BindRaw(Processor, &FProxyGenerationProcessor::ProxyGenerationFailed);
		}
	}

	bDisableTriangleOrderOptimization = (CVarTriangleOrderOptimization.GetValueOnGameThread() == 2);

	bUsingNvTriStrip = !bDisableTriangleOrderOptimization && (CVarTriangleOrderOptimization.GetValueOnGameThread() == 0);

	// Construct and cache the version string for the mesh utilities module.
	VersionString = FString::Printf(
		TEXT("%s%s%s"),
		MESH_UTILITIES_VER,
		MeshReduction ? *MeshReduction->GetVersionString() : TEXT(""),
		bUsingNvTriStrip ? TEXT("_NvTriStrip") : TEXT("")
		);
	bUsingSimplygon = VersionString.Contains(TEXT("Simplygon"));
}

void FMeshUtilities::ShutdownModule()
{
	MeshReduction = NULL;
	MeshMerging = NULL;
	VersionString.Empty();
}

bool FMeshUtilities::GenerateUniqueUVsForStaticMesh(const FRawMesh& RawMesh, int32 TextureResolution, TArray<FVector2D>& OutTexCoords) const
{
	// Create a copy of original mesh
	FRawMesh TempMesh = RawMesh;

	// Find overlapping corners for UV generator. Allow some threshold - this should not produce any error in a case if resulting
	// mesh will not merge these vertices.
	TMultiMap<int32, int32> OverlappingCorners;
	FindOverlappingCorners(OverlappingCorners, RawMesh, THRESH_POINTS_ARE_SAME);

	// Generate new UVs
	FLayoutUV Packer(&TempMesh, 0, 1, FMath::Clamp(TextureResolution / 4, 32, 512));
	Packer.FindCharts(OverlappingCorners);

	bool bPackSuccess = Packer.FindBestPacking();
	if (bPackSuccess)
	{
		Packer.CommitPackedUVs();
		// Save generated UVs
		OutTexCoords = TempMesh.WedgeTexCoords[1];
	}
	return bPackSuccess;
}

bool FMeshUtilities::GenerateUniqueUVsForSkeletalMesh(const FStaticLODModel& LODModel, int32 TextureResolution, TArray<FVector2D>& OutTexCoords) const
{
	// Get easy to use SkeletalMesh data
	TArray<FSoftSkinVertex> Vertices;
	FMultiSizeIndexContainerData IndexData;
	LODModel.GetVertices(Vertices);
	LODModel.MultiSizeIndexContainer.GetIndexBufferData(IndexData);

	int32 NumCorners = IndexData.Indices.Num();

	// Generate FRawMesh from FStaticLODModel
	FRawMesh TempMesh;
	TempMesh.WedgeIndices.AddUninitialized(NumCorners);
	TempMesh.WedgeTexCoords[0].AddUninitialized(NumCorners);
	TempMesh.VertexPositions.AddUninitialized(NumCorners);

	// Prepare vertex to wedge map
	// PrevCorner[i] points to previous corner which shares the same wedge
	TArray<int32> LastWedgeCorner;
	LastWedgeCorner.AddUninitialized(Vertices.Num());
	TArray<int32> PrevCorner;
	PrevCorner.AddUninitialized(NumCorners);
	for (int32 Index = 0; Index < Vertices.Num(); Index++)
	{
		LastWedgeCorner[Index] = -1;
	}

	for (int32 Index = 0; Index < NumCorners; Index++)
	{
		// Copy static vertex data
		int32 VertexIndex = IndexData.Indices[Index];
		FSoftSkinVertex& Vertex = Vertices[VertexIndex];
		TempMesh.WedgeIndices[Index] = Index; // rudimental data, not really used by FLayoutUV - but array size matters
		TempMesh.WedgeTexCoords[0][Index] = Vertex.UVs[0];
		TempMesh.VertexPositions[Index] = Vertex.Position;
		// Link all corners belonging to a single wedge into list
		int32 PrevCornerIndex = LastWedgeCorner[VertexIndex];
		LastWedgeCorner[VertexIndex] = Index;
		PrevCorner[Index] = PrevCornerIndex;
	}

	//	return GenerateUniqueUVsForStaticMesh(TempMesh, TextureResolution, OutTexCoords);

	// Build overlapping corners map
	TMultiMap<int32, int32> OverlappingCorners;
	for (int32 Index = 0; Index < NumCorners; Index++)
	{
		int VertexIndex = IndexData.Indices[Index];
		for (int32 CornerIndex = LastWedgeCorner[VertexIndex]; CornerIndex >= 0; CornerIndex = PrevCorner[CornerIndex])
		{
			if (CornerIndex != Index)
			{
				OverlappingCorners.Add(Index, CornerIndex);
			}
		}
	}

	// Generate new UVs
	FLayoutUV Packer(&TempMesh, 0, 1, FMath::Clamp(TextureResolution / 4, 32, 512));
	Packer.FindCharts(OverlappingCorners);

	bool bPackSuccess = Packer.FindBestPacking();
	if (bPackSuccess)
	{
		Packer.CommitPackedUVs();
		// Save generated UVs
		OutTexCoords = TempMesh.WedgeTexCoords[1];
	}
	return bPackSuccess;
}

void FMeshUtilities::CalculateTangents(const TArray<FVector>& InVertices, const TArray<uint32>& InIndices, const TArray<FVector2D>& InUVs, const TArray<uint32>& InSmoothingGroupIndices, const uint32 InTangentOptions, TArray<FVector>& OutTangentX, TArray<FVector>& OutTangentY, TArray<FVector>& OutNormals) const
{
	const float ComparisonThreshold = (InTangentOptions & ETangentOptions::IgnoreDegenerateTriangles ) ? THRESH_POINTS_ARE_SAME : 0.0f;

	TMultiMap<int32, int32> OverlappingCorners;
	FindOverlappingCorners(OverlappingCorners, InVertices, InIndices, ComparisonThreshold);
	ComputeTangents(InVertices, InIndices, InUVs, InSmoothingGroupIndices, OverlappingCorners, OutTangentX, OutTangentY, OutNormals, InTangentOptions);
}


#undef LOCTEXT_NAMESPACE


