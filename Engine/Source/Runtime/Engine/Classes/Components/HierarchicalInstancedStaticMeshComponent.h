// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/InstancedStaticMeshComponent.h"
#include "StaticMeshResources.h"

#include "HierarchicalInstancedStaticMeshComponent.generated.h"

class FClusterBuilder;

USTRUCT()
struct FClusterNode
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector BoundMin;
	UPROPERTY()
	int32 FirstChild;
	UPROPERTY()
	FVector BoundMax;
	UPROPERTY()
	int32 LastChild;
	UPROPERTY()
	int32 FirstInstance;
	UPROPERTY()
	int32 LastInstance;

	FClusterNode()
		: BoundMin(MAX_flt, MAX_flt, MAX_flt)
		, FirstChild(-1)
		, BoundMax(MIN_flt, MIN_flt, MIN_flt)
		, LastChild(-1)
		, FirstInstance(-1)
		, LastInstance(-1)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FClusterNode& NodeData)
	{
		// @warning BulkSerialize: FClusterNode is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << NodeData.BoundMin;
		Ar << NodeData.FirstChild;
		Ar << NodeData.BoundMax;
		Ar << NodeData.LastChild;
		Ar << NodeData.FirstInstance;
		Ar << NodeData.LastInstance;
		return Ar;
	}
};

class FAsyncBuildInstanceBuffer : public FNonAbandonableTask
{
public:
	class UHierarchicalInstancedStaticMeshComponent* Component;
	class UWorld* World;

	FAsyncBuildInstanceBuffer(class UHierarchicalInstancedStaticMeshComponent* InComponent, class UWorld* InWorld)
		: Component(InComponent)
		, World(InWorld)
	{
	}
	void DoWork();
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncBuildInstanceBuffer, STATGROUP_ThreadPoolAsyncTasks);
	}
	static const TCHAR *Name()
	{
		return TEXT("FAsyncBuildInstanceBuffer");
	}
};

UCLASS(ClassGroup=Rendering, meta=(BlueprintSpawnableComponent))
class ENGINE_API UHierarchicalInstancedStaticMeshComponent : public UInstancedStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

	~UHierarchicalInstancedStaticMeshComponent();

	TSharedPtr<TArray<FClusterNode>, ESPMode::ThreadSafe> ClusterTreePtr;

	// Temp hack, long term we will load data in the right format directly
	FAsyncTask<FAsyncBuildInstanceBuffer>* AsyncBuildInstanceBufferTask;

	// Prebuilt instance buffer, used mostly for grass. Long term instances will be stored directly in the correct format.
	FStaticMeshInstanceData WriteOncePrebuiltInstanceBuffer;
	
	// Table for remaping instances from cluster tree to PerInstanceSMData order
	UPROPERTY()
	TArray<int32> SortedInstances;

	// The number of instances in the ClusterTree. Subsequent instances will always be rendered.
	UPROPERTY()
	int32 NumBuiltInstances;

	// Normally equal to NumBuiltInstances, but can be lower if density scaling is in effect
	int32 NumBuiltRenderInstances;

	// Bounding box of any built instances (cached from the ClusterTree)
	UPROPERTY()
	FBox BuiltInstanceBounds;

	// Bounding box of any unbuilt instances
	UPROPERTY()
	FBox UnbuiltInstanceBounds;

	// Bounds of each individual unbuilt instance, used for LOD calculation
	UPROPERTY()
	TArray<FBox> UnbuiltInstanceBoundsList;

	// Enable for detail meshes that don't really affect the game. Disable for anything important.
	// Typically, this will be enabled for small meshes without collision (e.g. grass) and disabled for large meshes with collision (e.g. trees)
	UPROPERTY()
	uint32 bEnableDensityScaling:1;

	// Which instances have been removed by foliage density scaling?
	TBitArray<> ExcludedDueToDensityScaling;

	// The number of nodes in the occlusion layer
	UPROPERTY()
	int32 OcclusionLayerNumNodes;

	bool bIsAsyncBuilding;
	bool bDiscardAsyncBuildResults;
	bool bConcurrentRemoval;

	UPROPERTY()
	bool bDisableCollision;

	// Apply the results of the async build
	void ApplyBuildTreeAsync(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, TSharedRef<FClusterBuilder, ESPMode::ThreadSafe> Builder, double StartTime);

public:

	//Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void PostLoad() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& BoundTransform) const override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	// UInstancedStaticMesh interface
	virtual int32 AddInstance(const FTransform& InstanceTransform) override;
	virtual bool RemoveInstance(int32 InstanceIndex) override;
	virtual bool UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty = false, bool bTeleport = false) override;
	virtual void ClearInstances() override;
	virtual TArray<int32> GetInstancesOverlappingSphere(const FVector& Center, float Radius, bool bSphereInWorldSpace = true) const override;
	virtual TArray<int32> GetInstancesOverlappingBox(const FBox& Box, bool bBoxInWorldSpace = true) const override;

	/** Removes all the instances with indices specified in the InstancesToRemove array. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	bool RemoveInstances(const TArray<int32>& InstancesToRemove);

	/** Get the number of instances that overlap a given sphere */
	int32 GetOverlappingSphereCount(const FSphere& Sphere) const;
	/** Get the number of instances that overlap a given box */
	int32 GetOverlappingBoxCount(const FBox& Box) const;
	/** Get the transforms of instances inside the provided box */
	void GetOverlappingBoxTransforms(const FBox& Box, TArray<FTransform>& OutTransforms) const;

	virtual bool ShouldCreatePhysicsState() const override;

	virtual int32 GetNumRenderInstances() const override { return NumBuiltRenderInstances + UnbuiltInstanceBoundsList.Num(); }

	void BuildTree();
	void BuildTreeAsync();
	static void BuildTreeAnyThread(
		TArray<FMatrix>& InstanceTransforms, 
		const FBox& MeshBox,
		TArray<FClusterNode>& OutClusterTree,
		TArray<int32>& OutSortedInstances,
		TArray<int32>& OutInstanceReorderTable,
		int32& OutOcclusionLayerNum,
		int32 MaxInstancesPerLeaf
		);
	void AcceptPrebuiltTree(TArray<FClusterNode>& InClusterTree, int InOcclusionLayerNumNodes);
	bool IsAsyncBuilding() const { return bIsAsyncBuilding; }
	bool IsTreeFullyBuilt() const { return NumBuiltInstances == PerInstanceSMData.Num() && RemovedInstances.Num() == 0; }

	void FlushAsyncBuildInstanceBufferTask();

	/** Heuristic for the number of leaves in the tree **/
	int32 DesiredInstancesPerLeaf();

protected:
	/** Removes a single instance without extra work such as rebuilding the tree or marking render state dirty. */
	void RemoveInstanceInternal(int32 InstanceIndex);

	void UpdateInstanceTreeBoundsInternal(int32 InstanceIndex, const FBox& NewBounds);
	static void UpdateInstanceTreeBoundsInternal_RenderThread(TArray<FClusterNode>& ClusterTree, int32 InstanceIndex, const FBox& NewBounds);

	/** Gets and approximate number of verts for each LOD to generate heuristics **/
	int32 GetVertsForLOD(int32 LODIndex);
	/** Average number of instances per leaf **/
	float ActualInstancesPerLeaf();
	/** For testing, prints some stats after any kind of build **/
	void PostBuildStats();

	virtual void GetNavigationPerInstanceTransforms(const FBox& AreaBox, TArray<FTransform>& InstanceData) const override;
	virtual void PartialNavigationUpdate(int32 InstanceIdx) override;
	void FlushAccumulatedNavigationUpdates();
	mutable FBox AccumulatedNavigationDirtyArea;

protected:
	friend FStaticLightingTextureMapping_InstancedStaticMesh;
	friend FInstancedLightMap2D;
	friend FInstancedShadowMap2D;
	friend class FClusterBuilder;
};

