// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InstancedStaticMesh.h: Instanced static mesh header
=============================================================================*/

#pragma once

#include "PhysicsPublic.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"

#include "Misc/UObjectToken.h"
#include "Components/SplineMeshComponent.h"
#include "Components/ModelComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/DrawSphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/VectorFieldComponent.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Components/TimelineComponent.h"
#include "SlateBasics.h"
#include "NavDataGenerator.h"
#include "AI/Navigation/RecastHelpers.h"

#include "StaticMeshResources.h"
#include "StaticMeshLight.h"
#include "SpeedTreeWind.h"
#include "ComponentInstanceDataCache.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"

#if WITH_PHYSX
#include "PhysicsEngine/PhysXSupport.h"
#include "Collision/PhysXCollision.h"
#endif

#if WITH_EDITOR
#include "LightMap.h"
#include "ShadowMap.h"
#include "Logging/MessageLog.h"
#endif

#include "NavigationSystemHelpers.h"
#include "AI/Navigation/NavCollision.h"
#include "Components/InstancedStaticMeshComponent.h"

extern TAutoConsoleVariable<float> CVarFoliageMinimumScreenSize;
extern TAutoConsoleVariable<float> CVarFoliageLODDistanceScale;
extern TAutoConsoleVariable<float> CVarRandomLODRange;
extern TAutoConsoleVariable<int32> CVarMinLOD;


// This must match the maximum a user could specify in the material (see 
// FHLSLMaterialTranslator::TextureCoordinate), otherwise the material will attempt 
// to look up a texture coordinate we didn't provide an element for.
extern const int32 InstancedStaticMeshMaxTexCoord;

/*-----------------------------------------------------------------------------
	FStaticMeshInstanceBuffer
-----------------------------------------------------------------------------*/

/** A vertex buffer of positions. */
class FStaticMeshInstanceBuffer : public FVertexBuffer
{
public:

	/** Default constructor. */
	FStaticMeshInstanceBuffer(ERHIFeatureLevel::Type InFeatureLevel);

	/** Destructor. */
	~FStaticMeshInstanceBuffer();

	/** Delete existing resources */
	void CleanUp();

	/**
	 * Initializes the buffer with the component's data.
	 * @param InComponent - The owning component
	 * @param InHitProxies - Array of hit proxies for each instance, if desired.
	 */
	void Init(UInstancedStaticMeshComponent* InComponent, const TArray<TRefCountPtr<HHitProxy> >& InHitProxies);

	/**
	 * Initializes the buffer with the component's data.
	 * @param InComponent - The owning component; this need not have PerInstanceSMData, as we are taking a prebuilt instance buffer
	 * @param Other - instance data, this call assumes the memory, so this will be empty after the call
	 */
	void InitFromPreallocatedData(UInstancedStaticMeshComponent* InComponent, FStaticMeshInstanceData& Other);

	/** Propagates instance selection state and hit proxy colors */
	void SetPerInstanceEditorData(UInstancedStaticMeshComponent* InComponent, const TArray<TRefCountPtr<HHitProxy>>& InHitProxies);

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar, FStaticMeshInstanceBuffer& VertexBuffer);

	/**
	 * Specialized assignment operator, only used when importing LOD's. 
	 */
	void operator=(const FStaticMeshInstanceBuffer &Other);

	// Other accessors.
	FORCEINLINE uint32 GetStride() const
	{
		return Stride;
	}
	FORCEINLINE uint32 GetNumInstances() const
	{
		return NumInstances;
	}

	const void* GetRawData() const
	{
		return InstanceData->GetDataPointer();
	}

	const FInstanceStream* GetData() const
	{
		return InstanceData->GetData();
	}

	const FInstanceStream* GetInstance(int32 InstanceIndex) const
	{
		return InstanceData->GetInstanceWriteAddress(InstanceIndex);
	}


	// FRenderResource interface.
	virtual void InitRHI() override;
	virtual FString GetFriendlyName() const override { return TEXT("Static-mesh instances"); }

private:

	/** The vertex data storage type */
	FStaticMeshInstanceData* InstanceData;

	/** The cached vertex stride. */
	uint32 Stride;

	/** The cached number of instances. */
	uint32 NumInstances;

	/** Allocates the vertex data storage type. */
	void AllocateData();

	/** Accepts preallocated data; Other is left empty after the call because no memory is copied. */
	void AllocateData(FStaticMeshInstanceData& Other);

	void SetupCPUAccess(UInstancedStaticMeshComponent* InComponent);
};

/*-----------------------------------------------------------------------------
	FInstancedStaticMeshVertexFactory
-----------------------------------------------------------------------------*/

struct FInstancingUserData
{
	class FInstancedStaticMeshRenderData* RenderData;
	class FStaticMeshRenderData* MeshRenderData;

	int32 StartCullDistance;
	int32 EndCullDistance;

	int32 MinLOD;

	bool bRenderSelected;
	bool bRenderUnselected;
};

/**
 * A vertex factory for instanced static meshes
 */
struct FInstancedStaticMeshVertexFactory : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FInstancedStaticMeshVertexFactory);
public:
	struct DataType : public FLocalVertexFactory::DataType
	{
		/** The stream to read the mesh transform from. */
		FVertexStreamComponent InstanceOriginComponent;

		/** The stream to read the mesh transform from. */
		FVertexStreamComponent InstanceTransformComponent[3];

		/** The stream to read the Lightmap Bias and Random instance ID from. */
		FVertexStreamComponent InstanceLightmapAndShadowMapUVBiasComponent;
	};

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Modify compile environment to enable instancing
	 * @param OutEnvironment - shader compile environment to modify
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("USE_INSTANCING"),TEXT("1"));
		const bool bInstanced = RHISupportsInstancing(Platform);
		OutEnvironment.SetDefine(TEXT("USE_INSTANCING_EMULATED"), bInstanced ? TEXT("0") : TEXT("1"));
		OutEnvironment.SetDefine(TEXT("USE_DITHERED_LOD_TRANSITION"), ALLOW_DITHERED_LOD_FOR_INSTANCED_STATIC_MESHES ? TEXT("1") : TEXT("0"));
		FLocalVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const DataType& InData)
	{
		Data = InData;
		UpdateRHI();
	}

	/**
	 * Copy the data from another vertex factory
	 * @param Other - factory to copy from
	 */
	void Copy(const FInstancedStaticMeshVertexFactory& Other);

	// FRenderResource interface.
	virtual void InitRHI() override;

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	/** Make sure we account for changes in the signature of GetStaticBatchElementVisibility() */
	static CONSTEXPR uint32 NumBitsForVisibilityMask()
	{		
		return 8 * sizeof(decltype(((FInstancedStaticMeshVertexFactory*)nullptr)->GetStaticBatchElementVisibility(FSceneView(FSceneViewInitOptions()), nullptr)));
	}

	/**
	* Get a bitmask representing the visibility of each FMeshBatch element.
	*/
	virtual uint64 GetStaticBatchElementVisibility(const class FSceneView& View, const struct FMeshBatch* Batch) const override
	{
		uint32 NumElements = FMath::Min((uint32)Batch->Elements.Num(), NumBitsForVisibilityMask());
		return (1ULL << (uint64)NumElements) - 1ULL;
	}
#if ALLOW_DITHERED_LOD_FOR_INSTANCED_STATIC_MESHES
	virtual bool SupportsNullPixelShader() const override { return false; }
#endif

private:
	DataType Data;
};




class FInstancedStaticMeshVertexFactoryShaderParameters : public FLocalVertexFactoryShaderParameters
{
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		FLocalVertexFactoryShaderParameters::Bind(ParameterMap);

		InstancingFadeOutParamsParameter.Bind(ParameterMap, TEXT("InstancingFadeOutParams"));
		InstancingViewZCompareZeroParameter.Bind(ParameterMap, TEXT("InstancingViewZCompareZero"));
		InstancingViewZCompareOneParameter.Bind(ParameterMap, TEXT("InstancingViewZCompareOne"));
		InstancingViewZConstantParameter.Bind(ParameterMap, TEXT("InstancingViewZConstant"));
		InstancingWorldViewOriginZeroParameter.Bind(ParameterMap, TEXT("InstancingWorldViewOriginZero"));
		InstancingWorldViewOriginOneParameter.Bind(ParameterMap, TEXT("InstancingWorldViewOriginOne"));
		CPUInstanceOrigin.Bind(ParameterMap, TEXT("CPUInstanceOrigin"));
		CPUInstanceTransform.Bind(ParameterMap, TEXT("CPUInstanceTransform"));
		CPUInstanceLightmapAndShadowMapBias.Bind(ParameterMap, TEXT("CPUInstanceLightmapAndShadowMapBias"));
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* VertexShader,const class FVertexFactory* VertexFactory,const class FSceneView& View,const struct FMeshBatchElement& BatchElement,uint32 DataFlags) const override;

	void Serialize(FArchive& Ar) override
	{
		FLocalVertexFactoryShaderParameters::Serialize(Ar);
		Ar << InstancingFadeOutParamsParameter;
		Ar << InstancingViewZCompareZeroParameter;
		Ar << InstancingViewZCompareOneParameter;
		Ar << InstancingViewZConstantParameter;
		Ar << InstancingWorldViewOriginZeroParameter;
		Ar << InstancingWorldViewOriginOneParameter;
		Ar << CPUInstanceOrigin;
		Ar << CPUInstanceTransform;
		Ar << CPUInstanceLightmapAndShadowMapBias;
	}

	virtual uint32 GetSize() const override { return sizeof(*this); }

private:
	FShaderParameter InstancingFadeOutParamsParameter;
	FShaderParameter InstancingViewZCompareZeroParameter;
	FShaderParameter InstancingViewZCompareOneParameter;
	FShaderParameter InstancingViewZConstantParameter;
	FShaderParameter InstancingWorldViewOriginZeroParameter;
	FShaderParameter InstancingWorldViewOriginOneParameter;

	FShaderParameter CPUInstanceOrigin;
	FShaderParameter CPUInstanceTransform;
	FShaderParameter CPUInstanceLightmapAndShadowMapBias;
};

/*-----------------------------------------------------------------------------
	FPerInstanceRenderData
	Holds render data that can persist between scene proxy reconstruction
-----------------------------------------------------------------------------*/
struct FPerInstanceRenderData
{
	// Should be always constructed on main thread
	FPerInstanceRenderData(UInstancedStaticMeshComponent* InComponent, ERHIFeatureLevel::Type InFeaureLevel)
		: InstanceBuffer(InFeaureLevel)
		, CachedSelectionStamp(InComponent->SelectionStamp)
	{
		// Create hit proxies for each instance if the component wants
		if (GIsEditor && InComponent->bHasPerInstanceHitProxies)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FPerInstanceRenderData_HitProxies);
			HitProxies.Empty(InComponent->PerInstanceSMData.Num());
			for (int32 InstanceIdx=0; InstanceIdx < InComponent->PerInstanceSMData.Num(); InstanceIdx++)
			{
				HitProxies.Add(new HInstancedStaticMeshInstance(InComponent, InstanceIdx));
			}
		}
			
		// initialize the instance buffer from the component's instances
		InstanceBuffer.Init(InComponent, HitProxies);
		BeginInitResource(&InstanceBuffer);
	}

	FPerInstanceRenderData(UInstancedStaticMeshComponent* InComponent, FStaticMeshInstanceData& Other, ERHIFeatureLevel::Type InFeaureLevel)
		: InstanceBuffer(InFeaureLevel)
		, CachedSelectionStamp(0)
	{
		InstanceBuffer.InitFromPreallocatedData(InComponent, Other);
		BeginInitResource(&InstanceBuffer);
	}
	
	// Should be always destructed on render thread
	~FPerInstanceRenderData()
	{
		InstanceBuffer.ReleaseResource();
	}
		
	/** Instance buffer */
	FStaticMeshInstanceBuffer			InstanceBuffer;
	/** Hit proxies for the instances */
	TArray<TRefCountPtr<HHitProxy>>		HitProxies;
	/** Cached selection identifier from component */
	int32								CachedSelectionStamp;
};


/*-----------------------------------------------------------------------------
	FInstancedStaticMeshRenderData
-----------------------------------------------------------------------------*/

class FInstancedStaticMeshRenderData
{
public:

	FInstancedStaticMeshRenderData(UInstancedStaticMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel)
	  : Component(InComponent)
	  , PerInstanceRenderData(InComponent->PerInstanceRenderData)
	  , LODModels(Component->StaticMesh->RenderData->LODResources)
	  , FeatureLevel(InFeatureLevel)
	{
		// Allocate the vertex factories for each LOD
		InitVertexFactories();

		if (!PerInstanceRenderData.IsValid())
		{
			// initialize the instance buffer from the component's instances
			InComponent->PerInstanceRenderData = MakeShareable(new FPerInstanceRenderData(InComponent, InFeatureLevel));
			PerInstanceRenderData = InComponent->PerInstanceRenderData;
			InComponent->bPerInstanceRenderDataWasPrebuilt = false;
		}
		else if (PerInstanceRenderData->CachedSelectionStamp != InComponent->SelectionStamp)
		{
			// Update editor data bits in already existing instance buffer
			PerInstanceRenderData->InstanceBuffer.SetPerInstanceEditorData(InComponent, PerInstanceRenderData->HitProxies);
			PerInstanceRenderData->CachedSelectionStamp = InComponent->SelectionStamp;
			BeginUpdateResourceRHI(&PerInstanceRenderData->InstanceBuffer);
		}

		NumInstances = PerInstanceRenderData->InstanceBuffer.GetNumInstances();
		InitResources();
	}

	FInstancedStaticMeshRenderData(UInstancedStaticMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel, FStaticMeshInstanceData& Other)
		: Component(InComponent)
		, PerInstanceRenderData(InComponent->PerInstanceRenderData)
		, LODModels(Component->StaticMesh->RenderData->LODResources)
		, FeatureLevel(InFeatureLevel)
	{
		InitVertexFactories();
		// initialize the instance buffer from the prebuilt instances
		if (!PerInstanceRenderData.IsValid())
		{
			InComponent->PerInstanceRenderData = MakeShareable(new FPerInstanceRenderData(InComponent, Other, InFeatureLevel));
			PerInstanceRenderData = InComponent->PerInstanceRenderData;
			InComponent->bPerInstanceRenderDataWasPrebuilt = InComponent->PerInstanceSMData.Num() == 0;
		}
		NumInstances = PerInstanceRenderData->InstanceBuffer.GetNumInstances();
		InitResources();
	}

	~FInstancedStaticMeshRenderData()
	{
	}

	void InitResources()
	{
		// Initialize the static mesh's vertex factory.
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			CallInitStaticMeshVertexFactory,
			TArray<FInstancedStaticMeshVertexFactory>*,VertexFactories,&VertexFactories,
			FInstancedStaticMeshRenderData*,InstancedRenderData,this,
			UStaticMesh*,Parent,Component->StaticMesh,
		{
			InitStaticMeshVertexFactories( VertexFactories, InstancedRenderData, Parent );
		});

		for( int32 LODIndex=0;LODIndex<VertexFactories.Num();LODIndex++ )
		{
			BeginInitResource(&VertexFactories[LODIndex]);
		}

		// register SpeedTree wind with the scene
		if (Component->StaticMesh->SpeedTreeWind.IsValid())
		{
			for (int32 LODIndex = 0; LODIndex < LODModels.Num(); LODIndex++)
			{
				Component->GetScene()->AddSpeedTreeWind(&VertexFactories[LODIndex], Component->StaticMesh);
			}
		}
	}

	void ReleaseResources(FSceneInterface* Scene, const UStaticMesh* StaticMesh)
	{
		// unregister SpeedTree wind with the scene
		if (Scene && StaticMesh && StaticMesh->SpeedTreeWind.IsValid())
		{
			for (int32 LODIndex = 0; LODIndex < VertexFactories.Num(); LODIndex++)
			{
				Scene->RemoveSpeedTreeWind_RenderThread(&VertexFactories[LODIndex], StaticMesh);
			}
		}

		for( int32 LODIndex=0;LODIndex<VertexFactories.Num();LODIndex++ )
		{
			VertexFactories[LODIndex].ReleaseResource();
		}
	}

	static void InitStaticMeshVertexFactories(
		TArray<FInstancedStaticMeshVertexFactory>* VertexFactories,
		FInstancedStaticMeshRenderData* InstancedRenderData,
		UStaticMesh* Parent);

	/** Source component */
	UInstancedStaticMeshComponent* Component;

	/** Per instance render data, could be shared with component */
	TSharedPtr<FPerInstanceRenderData, ESPMode::ThreadSafe> PerInstanceRenderData;

	/** Vertex factory */
	TArray<FInstancedStaticMeshVertexFactory> VertexFactories;

	/** LOD render data from the static mesh. */
	TIndirectArray<FStaticMeshLODResources>& LODModels;

	/** Feature level used when creating instance data */
	ERHIFeatureLevel::Type FeatureLevel;

	/** Number of instances */
	int32 NumInstances;

private:

	void InitVertexFactories()
	{
		// Allocate the vertex factories for each LOD
		for( int32 LODIndex=0;LODIndex<LODModels.Num();LODIndex++ )
		{
			FInstancedStaticMeshVertexFactory* VertexFactory = new(VertexFactories)FInstancedStaticMeshVertexFactory;
			VertexFactory->SetFeatureLevel(FeatureLevel);
		}
	}

};


/*-----------------------------------------------------------------------------
	FInstancedStaticMeshSceneProxy
-----------------------------------------------------------------------------*/

class FInstancedStaticMeshSceneProxy : public FStaticMeshSceneProxy
{
public:

	FInstancedStaticMeshSceneProxy(UInstancedStaticMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel)
	:	FStaticMeshSceneProxy(InComponent)
	,	InstancedRenderData(InComponent, InFeatureLevel)
#if WITH_EDITOR
	,	bHasSelectedInstances(InComponent->SelectedInstances.Num() > 0)
#endif
	{
		SetupProxy(InComponent);
	}

	FInstancedStaticMeshSceneProxy(UInstancedStaticMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel, FStaticMeshInstanceData& Other)
		:	FStaticMeshSceneProxy(InComponent)
		,	InstancedRenderData(InComponent, InFeatureLevel, Other)
#if WITH_EDITOR
		,	bHasSelectedInstances(InComponent->SelectedInstances.Num() > 0)
#endif
	{
		SetupProxy(InComponent);
	}


	~FInstancedStaticMeshSceneProxy()
	{
		InstancedRenderData.ReleaseResources(&GetScene( ), StaticMesh);
	}

	// FPrimitiveSceneProxy interface.
	
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override
	{
		FPrimitiveViewRelevance Result;
		if(View->Family->EngineShowFlags.InstancedStaticMeshes)
		{
			Result = FStaticMeshSceneProxy::GetViewRelevance(View);
#if WITH_EDITOR
			// use dynamic path to render selected indices
			if( bHasSelectedInstances )
			{
				Result.bDynamicRelevance = true;
			}
#endif
		}
		return Result;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual int32 GetNumMeshBatches() const override;

	/** Sets up a shadow FMeshBatch for a specific LOD. */
	virtual bool GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch) const override;

	/** Sets up a FMeshBatch for a specific LOD and element. */
	virtual bool GetMeshElement(int32 LODIndex, int32 BatchIndex, int32 ElementIndex, uint8 InDepthPriorityGroup, const bool bUseSelectedMaterial, const bool bUseHoveredMaterial, FMeshBatch& OutMeshBatch) const override;

	/** Sets up a wireframe FMeshBatch for a specific LOD. */
	virtual bool GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch) const override;

	virtual void GetDistancefieldAtlasData(FBox& LocalVolumeBounds, FIntVector& OutBlockMin, FIntVector& OutBlockSize, bool& bOutBuiltAsIfTwoSided, bool& bMeshWasPlane, TArray<FMatrix>& ObjectLocalToWorldTransforms) const override;

	virtual void GetDistanceFieldInstanceInfo(int32& NumInstances, float& BoundsSurfaceArea) const override;

	/**
	 * Creates the hit proxies are used when DrawDynamicElements is called.
	 * Called in the game thread.
	 * @param OutHitProxies - Hit proxes which are created should be added to this array.
	 * @return The hit proxy to use by default for elements drawn by DrawDynamicElements.
	 */
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override
	{
		if( InstancedRenderData.PerInstanceRenderData->HitProxies.Num() )
		{
			// Add any per-instance hit proxies.
			OutHitProxies += InstancedRenderData.PerInstanceRenderData->HitProxies;

			// No default hit proxy.
			return NULL;
		}
		else
		{
			return FStaticMeshSceneProxy::CreateHitProxies(Component, OutHitProxies);
		}
	}

	virtual bool IsDetailMesh() const override
	{
		return true;
	}

protected:
	/** Per component render data */
	FInstancedStaticMeshRenderData InstancedRenderData;

#if WITH_EDITOR
	/* If we we have any selected instances */
	bool bHasSelectedInstances;
#else
	static const bool bHasSelectedInstances = false;
#endif

	/** LOD transition info. */
	FInstancingUserData UserData_AllInstances;
	FInstancingUserData UserData_SelectedInstances;
	FInstancingUserData UserData_DeselectedInstances;

	/** Common path for the Get*MeshElement functions */
	void SetupInstancedMeshBatch(int32 LODIndex, int32 BatchIndex, FMeshBatch& OutMeshBatch) const;

private:

	void SetupProxy(UInstancedStaticMeshComponent* InComponent)
	{
#if WITH_EDITOR
		if( bHasSelectedInstances )
		{
			// if we have selected indices, mark scene proxy as selected.
			SetSelection_GameThread(true);
		}
#endif
		// Make sure all the materials are okay to be rendered as an instanced mesh.
		for (int32 LODIndex = 0; LODIndex < LODs.Num(); LODIndex++)
		{
			FStaticMeshSceneProxy::FLODInfo& LODInfo = LODs[LODIndex];
			for (int32 SectionIndex = 0; SectionIndex < LODInfo.Sections.Num(); SectionIndex++)
			{
				FStaticMeshSceneProxy::FLODInfo::FSectionInfo& Section = LODInfo.Sections[SectionIndex];
				if (!Section.Material->CheckMaterialUsage_Concurrent(MATUSAGE_InstancedStaticMeshes))
				{
					Section.Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}
			}
		}

		const bool bInstanced = RHISupportsInstancing(GetFeatureLevelShaderPlatform(InstancedRenderData.FeatureLevel));

		// Copy the parameters for LOD - all instances
		UserData_AllInstances.MeshRenderData = InComponent->StaticMesh->RenderData;
		UserData_AllInstances.StartCullDistance = InComponent->InstanceStartCullDistance;
		UserData_AllInstances.EndCullDistance = InComponent->InstanceEndCullDistance;
		UserData_AllInstances.MinLOD = ClampedMinLOD;
		UserData_AllInstances.bRenderSelected = true;
		UserData_AllInstances.bRenderUnselected = true;
		UserData_AllInstances.RenderData = bInstanced ? nullptr : &InstancedRenderData;

		// selected only
		UserData_SelectedInstances = UserData_AllInstances;
		UserData_SelectedInstances.bRenderUnselected = false;

		// unselected only
		UserData_DeselectedInstances = UserData_AllInstances;
		UserData_DeselectedInstances.bRenderSelected = false;
	}
};

#if WITH_EDITOR
/*-----------------------------------------------------------------------------
	FInstancedStaticMeshStaticLightingMesh
-----------------------------------------------------------------------------*/

/**
 * A static lighting mesh class that transforms the points by the per-instance transform of an 
 * InstancedStaticMeshComponent
 */
class FStaticLightingMesh_InstancedStaticMesh : public FStaticMeshStaticLightingMesh
{
public:

	/** Initialization constructor. */
	FStaticLightingMesh_InstancedStaticMesh(const UInstancedStaticMeshComponent* InPrimitive, int32 LODIndex, int32 InstanceIndex, const TArray<ULightComponent*>& InRelevantLights)
		: FStaticMeshStaticLightingMesh(InPrimitive, LODIndex, InRelevantLights)
	{
		// override the local to world to combine the per instance transform with the component's standard transform
		SetLocalToWorld(InPrimitive->PerInstanceSMData[InstanceIndex].Transform * InPrimitive->ComponentToWorld.ToMatrixWithScale());
	}
};

/*-----------------------------------------------------------------------------
	FInstancedStaticMeshStaticLightingTextureMapping
-----------------------------------------------------------------------------*/


/** Represents a static mesh primitive with texture mapped static lighting. */
class FStaticLightingTextureMapping_InstancedStaticMesh : public FStaticMeshStaticLightingTextureMapping
{
public:
	/** Initialization constructor. */
	FStaticLightingTextureMapping_InstancedStaticMesh(UInstancedStaticMeshComponent* InPrimitive, int32 LODIndex, int32 InInstanceIndex, FStaticLightingMesh* InMesh, int32 InSizeX, int32 InSizeY, int32 InTextureCoordinateIndex, bool bPerformFullQualityRebuild)
		: FStaticMeshStaticLightingTextureMapping(InPrimitive, LODIndex, InMesh, InSizeX, InSizeY, InTextureCoordinateIndex, bPerformFullQualityRebuild)
		, InstanceIndex(InInstanceIndex)
		, QuantizedData(nullptr)
		, ShadowMapData()
		, bComplete(false)
	{
	}

	// FStaticLightingTextureMapping interface
	virtual void Apply(FQuantizedLightmapData* InQuantizedData, const TMap<ULightComponent*, FShadowMapData2D*>& InShadowMapData) override
	{
		check(bComplete == false);

		UInstancedStaticMeshComponent* InstancedComponent = Cast<UInstancedStaticMeshComponent>(Primitive.Get());

		if (InstancedComponent)
		{
			// Save the static lighting until all of the component's static lighting has been built.
			QuantizedData = TUniquePtr<FQuantizedLightmapData>(InQuantizedData);
			ShadowMapData.Empty(InShadowMapData.Num());
			for (auto& ShadowDataPair : InShadowMapData)
			{
				ShadowMapData.Add(ShadowDataPair.Key, TUniquePtr<FShadowMapData2D>(ShadowDataPair.Value));
			}

			InstancedComponent->ApplyLightMapping(this);
		}

		bComplete = true;
	}

	virtual bool DebugThisMapping() const override
	{
		return false;
	}

	virtual FString GetDescription() const override
	{
		return FString(TEXT("InstancedSMLightingMapping"));
	}

private:
	friend class UInstancedStaticMeshComponent;

	/** The instance of the primitive this mapping represents. */
	const int32 InstanceIndex;

	// Light/shadow map data stored until all instances for this component are processed
	// so we can apply them all into one light/shadowmap
	TUniquePtr<FQuantizedLightmapData> QuantizedData;
	TMap<ULightComponent*, TUniquePtr<FShadowMapData2D>> ShadowMapData;

	/** Has this mapping already been completed? */
	bool bComplete;
};

#endif

/**
 * Structure that maps a component to it's lighting/instancing specific data which must be the same
 * between all instances that are bound to that component.
 */
struct FComponentInstanceSharingData
{
	/** The component that is associated (owns) this data */
	UInstancedStaticMeshComponent* Component;

	/** Light map texture */
	UTexture* LightMapTexture;

	/** Shadow map texture (or NULL if no shadow map) */
	UTexture* ShadowMapTexture;


	FComponentInstanceSharingData()
		: Component( NULL ),
		  LightMapTexture( NULL ),
		  ShadowMapTexture( NULL )
	{
	}
};


/**
 * Helper struct to hold information about what components use what lightmap textures
 */
struct FComponentInstancedLightmapData
{
	/** List of all original components and their original instances containing */
	TMap<UInstancedStaticMeshComponent*, TArray<FInstancedStaticMeshInstanceData> > ComponentInstances;

	/** List of new components */
	TArray< FComponentInstanceSharingData > SharingData;
};

