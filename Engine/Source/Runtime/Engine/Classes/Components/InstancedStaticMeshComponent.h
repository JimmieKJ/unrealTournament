// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/StaticMeshComponent.h"

#include "InstancedStaticMeshComponent.generated.h"

DECLARE_STATS_GROUP(TEXT("Foliage"), STATGROUP_Foliage, STATCAT_Advanced);

class FStaticLightingTextureMapping_InstancedStaticMesh;
class FInstancedLightMap2D;
class FInstancedShadowMap2D;
struct FPerInstanceRenderData;

USTRUCT()
struct FInstancedStaticMeshInstanceData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Instances)
	FMatrix Transform;

	UPROPERTY()
	FVector2D LightmapUVBias;

	UPROPERTY()
	FVector2D ShadowmapUVBias;


	FInstancedStaticMeshInstanceData()
		: Transform(FMatrix::Identity)
		, LightmapUVBias(ForceInit)
		, ShadowmapUVBias(ForceInit)
	{
	}


	friend FArchive& operator<<(FArchive& Ar, FInstancedStaticMeshInstanceData& InstanceData)
	{
		// @warning BulkSerialize: FInstancedStaticMeshInstanceData is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << InstanceData.Transform << InstanceData.LightmapUVBias << InstanceData.ShadowmapUVBias;
		return Ar;
	}
	
};

USTRUCT()
struct FInstancedStaticMeshMappingInfo
{
	GENERATED_USTRUCT_BODY()

	FStaticLightingTextureMapping_InstancedStaticMesh* Mapping;

	FInstancedStaticMeshMappingInfo()
		: Mapping(nullptr)
	{
	}
};

/** A component that efficiently renders multiple instances of the same StaticMesh. */
UCLASS(ClassGroup=Rendering, meta=(BlueprintSpawnableComponent))
class ENGINE_API UInstancedStaticMeshComponent : public UStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** Array of instances, bulk serialized */
	UPROPERTY(EditAnywhere, Transient, DuplicateTransient, DisplayName="Instances", Category=Instances, meta=(MakeEditWidget=true))
	TArray<FInstancedStaticMeshInstanceData> PerInstanceSMData;

	/** Value used to seed the random number stream that generates random numbers for each of this mesh's instances.
		The random number is stored in a buffer accessible to materials through the PerInstanceRandom expression.  If
		this is set to zero (default), it will be populated automatically by the editor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InstancedStaticMeshComponent)
	int32 InstancingRandomSeed;

	/** Distance from camera at which each instance begins to fade out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Culling)
	int32 InstanceStartCullDistance;

	/** Distance from camera at which each instance completely fades out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Culling)
	int32 InstanceEndCullDistance;

	/** Mapping from PerInstanceSMData order to instance render buffer order. If empty, the PerInstanceSMData order is used. */
	UPROPERTY()
	TArray<int32> InstanceReorderTable;

	// The render indices of any removed items we should not render.
	UPROPERTY()
	TArray<int32> RemovedInstances;

	/** Tracks outstanding proxysize, as this is a bit hard to do with the fire-and-forget grass. */
	SIZE_T ProxySize;

	/** Add an instance to this component. Transform is given in local space of this component.  */
	UFUNCTION(BlueprintCallable, Category="Components|InstancedStaticMesh")
	virtual int32 AddInstance(const FTransform& InstanceTransform);

	/** Add an instance to this component. Transform is given in world space. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	int32 AddInstanceWorldSpace(const FTransform& WorldTransform);

	/** Get the transform for the instance specified. Instance is returned in local space of this component unless bWorldSpace is set.  Returns True on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	bool GetInstanceTransform(int32 InstanceIndex, FTransform& OutInstanceTransform, bool bWorldSpace = false) const;
	
	/** Update the transform for the instance specified. Instance is given in local space of this component unless bWorldSpace is set.  Returns True on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual bool UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace=false, bool bMarkRenderStateDirty = false);

	/** Remove the instance specified. Returns True on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual bool RemoveInstance(int32 InstanceIndex);
	
	/** Clear all instances being rendered by this component */
	UFUNCTION(BlueprintCallable, Category="Components|InstancedStaticMesh")
	virtual void ClearInstances();
	
	/** Get the number of instances in this component */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	int32 GetInstanceCount() const;

	/** Sets the fading start and culling end distances for this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	void SetCullDistances(int32 StartCullDistance, int32 EndCullDistance);

	virtual bool ShouldCreatePhysicsState() const override;

public:
	/** Render data will be initialized once we create scene proxy for this component
	 *  Released on the rendering thread
	 */
	TSharedPtr<FPerInstanceRenderData, ESPMode::ThreadSafe> PerInstanceRenderData;
	/** This was prebuilt for grass, never destroy it. */
	bool bPerInstanceRenderDataWasPrebuilt;
		
#if WITH_EDITOR
	/** One bit per instance if the instance is selected. */
	TBitArray<> SelectedInstances;
#endif
	/** Incremented when instance selection is changed */
	int32 SelectionStamp;
	
#if WITH_PHYSX
	/** Aggregate physx representation of the instances' bodies. */
	TArray<physx::PxAggregate*> Aggregates;
#endif	//WITH_PHYSX

	/** Physics representation of the instance bodies */
	TArray<FBodyInstance*> InstanceBodies;

	/** Serialization of all the InstanceBodies. Helps speed up physics creation time*/
	UPROPERTY()
	class UPhysicsSerializer* PhysicsSerializer;

	// Begin UActorComponent interface 
	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;
	virtual FName GetComponentInstanceDataType() const override;
	// End UActorComponent interface 

	// Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void CreatePhysicsState() override;
	virtual void DestroyPhysicsState() override;
	virtual bool CanEditSimulatePhysics() override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& BoundTransform) const override;
	virtual bool SupportsStaticLighting() const override { return true; }
#if WITH_EDITOR
	virtual void GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options) override;
#endif
	virtual void GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const override;
	
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
	// End UPrimitiveComponent Interface
	
	// Begin UNavRelevantInterface interface.
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	// End UPrimitiveComponent interface.
	
	//Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
	//End UObject Interface

	/** Applies the cached component instance data to a newly blueprint constructed component */
	void ApplyComponentInstanceData(class FInstancedStaticMeshComponentInstanceData* ComponentInstanceData);

	/** Check to see if an instance is selected */
	bool IsInstanceSelected(int32 InInstanceIndex) const;

	/** Select/deselect an instance or group of instances */
	void SelectInstance(bool bInSelected, int32 InInstanceIndex, int32 InInstanceCount = 1);

	/** Deselect all instances */
	void ClearInstanceSelection();

	/** 
	 * Transfers ownership of instance render data to a render thread 
	 * instance render data will be released in scene proxy dtor or on render thread task
	 */
	void ReleasePerInstanceRenderData();

private:
	/** Creates body instances for all instances owned by this component */
	void CreateAllInstanceBodies();

	/** Terminate all body instances owned by this component */
	void ClearAllInstanceBodies();

	/** Sets up new instance data to sensible defaults, creates physics counterparts if possible */
	void SetupNewInstanceData(FInstancedStaticMeshInstanceData& InOutNewInstanceData, int32 InInstanceIndex, const FTransform& InInstanceTransform);
	
protected:
	/** Request to navigation system to update only part of navmesh occupied by specified instance */
	virtual void PartialNavigationUpdate(int32 InstanceIdx);
	
	/** Handles request from navigation system to gather instance transforms in a specific area box */
	virtual void GetNavigationPerInstanceTransforms(const FBox& AreaBox, TArray<FTransform>& InstanceData) const;

	/** Initializes the body instance for the specified instance of the static mesh*/
	void InitInstanceBody(int32 InstanceIdx, FBodyInstance* BodyInstance);

	/** Number of pending lightmaps still to be calculated (Apply()'d) */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	int32 NumPendingLightmaps;

	/** The mappings for all the instances of this component */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TArray<FInstancedStaticMeshMappingInfo> CachedMappings;

	void ApplyLightMapping(FStaticLightingTextureMapping_InstancedStaticMesh* InMapping);

	friend FStaticLightingTextureMapping_InstancedStaticMesh;
	friend FInstancedLightMap2D;
	friend FInstancedShadowMap2D;
};

/** InstancedStaticMeshInstance hit proxy */
struct HInstancedStaticMeshInstance : public HHitProxy
{
	UInstancedStaticMeshComponent* Component;
	int32 InstanceIndex;

	DECLARE_HIT_PROXY(ENGINE_API);
	HInstancedStaticMeshInstance(UInstancedStaticMeshComponent* InComponent, int32 InInstanceIndex) : HHitProxy(HPP_World), Component(InComponent), InstanceIndex(InInstanceIndex) {}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};
