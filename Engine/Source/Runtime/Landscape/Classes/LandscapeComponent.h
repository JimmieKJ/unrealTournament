// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LightMap.h"
#include "ShadowMap.h"

#include "SceneTypes.h"
#include "StaticLighting.h"
#include "Components/PrimitiveComponent.h"
#include "LandscapeGrassType.h"

#include "LandscapeComponent.generated.h"

class ULandscapeLayerInfoObject;
class ULandscapeInfo;
class ALandscapeProxy;
struct FEngineShowFlags;
struct FConvexVolume;

class FLandscapeComponentDerivedData
{
	/** The compressed Landscape component data for mobile rendering. Serialized to disk. 
	    On device, freed once it has been decompressed. */
	TArray<uint8> CompressedLandscapeData;

public:
	/** Returns true if there is any valid platform data */
	bool HasValidPlatformData() const
	{
		return CompressedLandscapeData.Num() != 0;
	}

	/** Initializes the compressed data from an uncompressed source. */
	void InitializeFromUncompressedData(const TArray<uint8>& UncompressedData);

	/** Decompresses and returns the data. Also frees the compressed data from memory when running with cooked data */
	void GetUncompressedData(TArray<uint8>& OutUncompressedData);

	/** Constructs a key string for the DDC that uniquely identifies a the Landscape component's derived data. */
	static FString GetDDCKeyString(const FGuid& StateId);

	/** Loads the platform data from DDC */
	bool LoadFromDDC(const FGuid& StateId);

	/** Saves the compressed platform data to the DDC */
	void SaveToDDC(const FGuid& StateId);

	/* Serializer */
	friend FArchive& operator<<(FArchive& Ar, FLandscapeComponentDerivedData& Data);
};

/* Used to uniquely reference a landscape vertex in a component, and generate a key suitable for a TMap. */
struct FLandscapeVertexRef
{
	FLandscapeVertexRef(int16 InX, int16 InY, int8 InSubX, int8 InSubY)
	: X(InX)
	, Y(InY)
	, SubX(InSubX)
	, SubY(InSubY)
	{}
	int16 X;
	int16 Y;
	int8 SubX;
	int8 SubY;

	uint64 MakeKey() const
	{
		return (uint64)X << 32 | (uint64)Y << 16 | (uint64)SubX << 8 | (uint64)SubY;
	}
};

/** Stores information about which weightmap texture and channel each layer is stored */
USTRUCT()
struct FWeightmapLayerAllocationInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	ULandscapeLayerInfoObject* LayerInfo;

	UPROPERTY()
	uint8 WeightmapTextureIndex;

	UPROPERTY()
	uint8 WeightmapTextureChannel;

	/** Only relevant in non-editor builds, this indicates which channel in the data array is this layer...must be > 1 to be valid, the first two are height **/
	UPROPERTY()
	uint8 GrassMapChannelIndex;

	FWeightmapLayerAllocationInfo()
		: WeightmapTextureIndex(0)
		, WeightmapTextureChannel(0)
		, GrassMapChannelIndex(0)
	{
	}


	FWeightmapLayerAllocationInfo(ULandscapeLayerInfoObject* InLayerInfo)
		:	LayerInfo(InLayerInfo)
		,	WeightmapTextureIndex(255)	// Indicates an invalid allocation
		,	WeightmapTextureChannel(255)
		,	GrassMapChannelIndex(0) // Indicates an invalid allocation
	{
	}
	
	FName GetLayerName() const;
};

struct FLandscapeComponentGrassData
{
	FGuid MaterialStateId;
	TArray<uint16> HeightData;
	TMap<ULandscapeGrassType*, TArray<uint8>> WeightData;

	FLandscapeComponentGrassData() {}

	FLandscapeComponentGrassData(FGuid& InMaterialStateId)
	: MaterialStateId(InMaterialStateId)
	{}

	bool HasData()
	{
		return HeightData.Num() > 0;
	}

	SIZE_T GetAllocatedSize() const
	{
		SIZE_T WeightSize = 0; 
		for (auto It = WeightData.CreateConstIterator(); It; ++It)
		{
			WeightSize += It.Value().GetAllocatedSize();
		}
		return sizeof(*this) + HeightData.GetAllocatedSize() + WeightData.GetAllocatedSize() + WeightSize;
	}

	friend FArchive& operator<<(FArchive& Ar, FLandscapeComponentGrassData& Data);
};

UCLASS(hidecategories=(Display, Attachment, Physics, Debug, Collision, Movement, Rendering, PrimitiveComponent, Object, Transform), showcategories=("Rendering|Material"), MinimalAPI)
class ULandscapeComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()
	
	/** X offset from global components grid origin (in quads) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 SectionBaseX;

	/** Y offset from global components grid origin (in quads) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 SectionBaseY;

	/** Total number of quads for this component, has to be >0 */
	UPROPERTY()
	int32 ComponentSizeQuads;

	/** Number of quads for a subsection of the component. SubsectionSizeQuads+1 must be a power of two. */
	UPROPERTY()
	int32 SubsectionSizeQuads;

	/** Number of subsections in X or Y axis */
	UPROPERTY()
	int32 NumSubsections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LandscapeComponent)
	class UMaterialInterface* OverrideMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LandscapeComponent, AdvancedDisplay)
	class UMaterialInterface* OverrideHoleMaterial;

	UPROPERTY(TextExportTransient)
	class UMaterialInstanceConstant* MaterialInstance;

	/** List of layers, and the weightmap and channel they are stored */
	UPROPERTY()
	TArray<struct FWeightmapLayerAllocationInfo> WeightmapLayerAllocations;

	/** Weightmap texture reference */
	UPROPERTY(TextExportTransient)
	TArray<class UTexture2D*> WeightmapTextures;

	/** XYOffsetmap texture reference */
	UPROPERTY(TextExportTransient)
	class UTexture2D* XYOffsetmapTexture;

	/** UV offset to component's weightmap data from component local coordinates*/
	UPROPERTY()
	FVector4 WeightmapScaleBias;

	/** U or V offset into the weightmap for the first subsection, in texture UV space */
	UPROPERTY()
	float WeightmapSubsectionOffset;

	/** UV offset to Heightmap data from component local coordinates */
	UPROPERTY()
	FVector4 HeightmapScaleBias;

	/** Heightmap texture reference */
	UPROPERTY(TextExportTransient)
	class UTexture2D* HeightmapTexture;

	/** Cached local-space bounding box, created at heightmap update time */
	UPROPERTY()
	FBox CachedLocalBox;

	/** Reference to associated collision component */
	UPROPERTY()
	TLazyObjectPtr<class ULandscapeHeightfieldCollisionComponent> CollisionComponent;

private:
#if WITH_EDITORONLY_DATA
	/** Unique ID for this component, used for caching during distributed lighting */
	UPROPERTY()
	FGuid LightingGuid;

#endif // WITH_EDITORONLY_DATA
public:
	/**	INTERNAL: Array of lights that don't apply to the terrain component.		*/
	UPROPERTY()
	TArray<FGuid> IrrelevantLights;

	/** Reference to the texture lightmap resource. */
	FLightMapRef LightMap;

	FShadowMapRef ShadowMap;

	/** Heightfield mipmap used to generate collision */
	UPROPERTY(EditAnywhere, Category=LandscapeComponent)
	int32 CollisionMipLevel;

	/** StaticLightingResolution overriding per component, default value 0 means no overriding */
	UPROPERTY(EditAnywhere, Category=LandscapeComponent)
	float StaticLightingResolution;

	/** Forced LOD level to use when rendering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 ForcedLOD;

	/** LOD level Bias to use when rendering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 LODBias;

	UPROPERTY()
	FGuid StateId;

	/** The Material Guid that used when baking, to detect material recompilations */
	UPROPERTY()
	FGuid BakedTextureMaterialGuid;

	/** Pre-baked Base Color texture for use by distance field GI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = BakedTextures)
	UTexture2D* GIBakedBaseColorTexture;

#if WITH_EDITORONLY_DATA
	/** LOD level Bias to use when lighting buidling via lightmass, -1 Means automatic LOD calculation based on ForcedLOD + LODBias */
	UPROPERTY(EditAnywhere, Category=LandscapeComponent)
	int32 LightingLODBias;

	UPROPERTY(Transient, DuplicateTransient)
	UTexture2D* SelectDataTexture; // Data texture used for selection mask

	/** Pointer to data shared with the render thread, used by the editor tools */
	struct FLandscapeEditToolRenderData* EditToolRenderData;

	/** Hash of source for ES2 generated data. Used for mobile preview to determine if we need to re-generate ES2 pixel data. */
	UPROPERTY(Transient, DuplicateTransient)
	FGuid MobilePixelDataSourceHash;
#endif

	/** For ES2 */
	UPROPERTY()
	uint8 MobileBlendableLayerMask;

	/** Material interface used for ES2. Serialized only when cooking or loading cooked builds. */
	UPROPERTY(Transient, DuplicateTransient)
	UMaterialInterface* MobileMaterialInterface;

	/** Generated weight/normal map texture used for ES2. Serialized only when cooking or loading cooked builds. */
	UPROPERTY(Transient, DuplicateTransient)
	UTexture2D* MobileWeightNormalmapTexture;

public:
	/** Platform Data where don't support texture sampling in vertex buffer */
	FLandscapeComponentDerivedData PlatformData;

	/** Grass data for generation **/
	TSharedRef<FLandscapeComponentGrassData, ESPMode::ThreadSafe> GrassData;

	virtual ~ULandscapeComponent();

	// Begin UObject interface.	
	virtual void PostInitProperties() override;	
	virtual void Serialize(FArchive& Ar) override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	virtual void BeginDestroy() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PostEditUndo() override;
	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End UObject interface

	/** Fix up component layers, weightmaps
	 */
	LANDSCAPE_API void FixupWeightmaps();
	
	// Begin UPrimitiveComponent interface.
	virtual bool GetLightMapResolution( int32& Width, int32& Height ) const override;
	virtual void GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const override;
	virtual void GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options) override;
#endif
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual ELightMapInteractionType GetStaticLightingType() const override { return LMIT_Texture;	}
	virtual void GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const override;

#if WITH_EDITOR
	virtual int32 GetNumMaterials() const override;
	virtual class UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual void SetMaterial(int32 ElementIndex, class UMaterialInterface* Material) override;
	virtual bool ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
	virtual bool ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
#endif
	// End UPrimitiveComponent interface.

	// Begin USceneComponent interface.
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// End USceneComponent interface.

	// Begin UActorComponent interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	// End UActorComponent interface.


#if WITH_EDITOR
	/** @todo document */
	LANDSCAPE_API ULandscapeInfo* GetLandscapeInfo(bool bSpawnNewActor = true) const;

	/** @todo document */
	void DeleteLayer(ULandscapeLayerInfoObject* LayerInfo, struct FLandscapeEditDataInterface* LandscapeEdit);

	void ReplaceLayer(ULandscapeLayerInfoObject* FromLayerInfo, ULandscapeLayerInfoObject* ToLayerInfo, struct FLandscapeEditDataInterface* LandscapeEdit);
	
	void GeneratePlatformVertexData();
	void GeneratePlatformPixelData(bool bIsCooking);

	/** Creates and destroys cooked grass data stored in the map */
	void RenderGrassMap();
	void RemoveGrassMap();

	/* Could a grassmap currently be generated, disregarding whether our textures are streamed in? */
	bool CanRenderGrassMap() const;

	/* Are the textures we need to render a grassmap currently streamed in? */
	bool AreTexturesStreamedForGrassMapRender() const;

	/* Is the grassmap data outdated, eg by a material */
	bool IsGrassMapOutdated() const;

	/* Serialize all hashes/guids that record the current state of this component */
	void SerializeStateHashes(FArchive& Ar);
#endif

	/** @todo document */
	virtual void InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly) override;


	/** Get the landscape actor associated with this component. */
	class ALandscape* GetLandscapeActor() const;

	/** Get the level in which the owning actor resides */
	class ULevel* GetLevel() const;

	/** Returns all generated textures and material instances used by this component. */
	LANDSCAPE_API void GetGeneratedTexturesAndMaterialInstances(TArray<UObject*>& OutTexturesAndMaterials) const;

	/** @todo document */
	LANDSCAPE_API ALandscapeProxy* GetLandscapeProxy() const;

	/** @return Component section base as FIntPoint */
	LANDSCAPE_API FIntPoint GetSectionBase() const; 

	/** @param InSectionBase new section base for a component */
	LANDSCAPE_API void SetSectionBase(FIntPoint InSectionBase);

	/** @todo document */
	TMap< UTexture2D*,struct FLandscapeWeightmapUsage >& GetWeightmapUsageMap();

	/** @todo document */
	const FGuid& GetLightingGuid() const
	{
#if WITH_EDITORONLY_DATA
		return LightingGuid;
#else
		static const FGuid NullGuid( 0, 0, 0, 0 );
		return NullGuid;
#endif // WITH_EDITORONLY_DATA
	}

	/** @todo document */
	void SetLightingGuid()
	{
#if WITH_EDITORONLY_DATA
		LightingGuid = FGuid::NewGuid();
#endif // WITH_EDITORONLY_DATA
	}

#if WITH_EDITOR


	/** Initialize the landscape component */
	LANDSCAPE_API void Init(int32 InBaseX,int32 InBaseY,int32 InComponentSizeQuads, int32 InNumSubsections,int32 InSubsectionSizeQuads);

	/**
	 * Recalculate cached bounds using height values.
	 */
	LANDSCAPE_API void UpdateCachedBounds();

	/**
	 * Update the MaterialInstance parameters to match the layer and weightmaps for this component
	 * Creates the MaterialInstance if it doesn't exist.
	 */
	LANDSCAPE_API void UpdateMaterialInstances();

	/** Helper function for UpdateMaterialInstance to get Material without set parameters */
	UMaterialInstanceConstant* GetCombinationMaterial(bool bMobile = false);
	/**
	 * Generate mipmaps for height and tangent data.
	 * @param HeightmapTextureMipData - array of pointers to the locked mip data.
	 *           This should only include the mips that are generated directly from this component's data
	 *           ie where each subsection has at least 2 vertices.
	* @param ComponentX1 - region of texture to update in component space, MAX_int32 meant end of X component in ALandscape::Import()
	* @param ComponentY1 - region of texture to update in component space, MAX_int32 meant end of Y component in ALandscape::Import()
	* @param ComponentX2 (optional) - region of texture to update in component space
	* @param ComponentY2 (optional) - region of texture to update in component space
	* @param TextureDataInfo - FLandscapeTextureDataInfo pointer, to notify of the mip data region updated.
	 */
	void GenerateHeightmapMips(TArray<FColor*>& HeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32,struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);

	/**
	 * Generate empty mipmaps for weightmap
	 */
	LANDSCAPE_API static void CreateEmptyTextureMips(UTexture2D* Texture, bool bClear = false);

	/**
	 * Generate mipmaps for weightmap
	 * Assumes all weightmaps are unique to this component.
	 * @param WeightmapTextureBaseMipData: array of pointers to each of the weightmaps' base locked mip data.
	 */
	template<typename DataType>

	/** @todo document */
	static void GenerateMipsTempl(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, DataType* BaseMipData);

	/** @todo document */
	static void GenerateWeightmapMips(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, FColor* BaseMipData);

	/**
	 * Update mipmaps for existing weightmap texture
	 */
	template<typename DataType>

	/** @todo document */
	static void UpdateMipsTempl(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, TArray<DataType*>& WeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);

	/** @todo document */
	LANDSCAPE_API static void UpdateWeightmapMips(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, TArray<FColor*>& WeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);

	/** @todo document */
	static void UpdateDataMips(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* Texture, TArray<uint8*>& TextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, struct FLandscapeTextureDataInfo* TextureDataInfo=NULL);

	/**
	 * Create or updatescollision component height data
	 * @param HeightmapTextureMipData: heightmap data
	 * @param ComponentX1, ComponentY1, ComponentX2, ComponentY2: region to update
	 * @param Whether to update bounds from render component.
	 */
	void UpdateCollisionHeightData(const FColor* HeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, bool bUpdateBounds=false, const FColor* XYOffsetTextureMipData = NULL, bool bRebuild=false);

	/**
	 * Update collision component dominant layer data
	 * @param WeightmapTextureMipData: weightmap data
	 * @param ComponentX1, ComponentY1, ComponentX2, ComponentY2: region to update
	 * @param Whether to update bounds from render component.
	 */
	void UpdateCollisionLayerData(TArray<FColor*>& WeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32);

	/**
	 * Update collision component dominant layer data for the whole component, locking and unlocking the weightmap textures.
	 */
	LANDSCAPE_API void UpdateCollisionLayerData();

	/**
	 * Create weightmaps for this component for the layers specified in the WeightmapLayerAllocations array
	 */
	void ReallocateWeightmaps(struct FLandscapeEditDataInterface* DataInterface=NULL);

	/** Returns the actor's LandscapeMaterial, or the Component's OverrideLandscapeMaterial if set */
	LANDSCAPE_API UMaterialInterface* GetLandscapeMaterial() const;

	/** Returns the actor's LandscapeHoleMaterial, or the Component's OverrideLandscapeHoleMaterial if set */
	LANDSCAPE_API UMaterialInterface* GetLandscapeHoleMaterial() const;

	/** Returns true if this component has visibility painted */
	LANDSCAPE_API bool ComponentHasVisibilityPainted() const;

	/**
	 * Generate a key for this component's layer allocations to use with MaterialInstanceConstantMap.
	 */
	FString GetLayerAllocationKey(UMaterialInterface* LandscapeMaterial, bool bMobile = false) const;

	/** @todo document */
	void GetLayerDebugColorKey(int32& R, int32& G, int32& B) const;

	/** @todo document */
	void RemoveInvalidWeightmaps();

	/** @todo document */
	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;

	/** @todo document */
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;

	/** @todo document */
	LANDSCAPE_API void InitHeightmapData(TArray<FColor>& Heights, bool bUpdateCollision);

	/** @todo document */
	LANDSCAPE_API void InitWeightmapData(TArray<class ULandscapeLayerInfoObject*>& LayerInfos, TArray<TArray<uint8> >& Weights);

	/** @todo document */
	LANDSCAPE_API float GetLayerWeightAtLocation( const FVector& InLocation, ULandscapeLayerInfoObject* LayerInfo, TArray<uint8>* LayerCache=NULL );

	/** Extends passed region with this component section size */
	LANDSCAPE_API void GetComponentExtent(int32& MinX, int32& MinY, int32& MaxX, int32& MaxY) const;

	/** Updates navigation properties to match landscape's master switch */
	void UpdateNavigationRelevance();
#endif

	friend class FLandscapeComponentSceneProxy;
	friend struct FLandscapeComponentDataInterface;

	void SetLOD(bool bForced, int32 InLODValue);

protected:

	/** Whether the component type supports static lighting. */
	virtual bool SupportsStaticLighting() const override
	{
		return true;
	}
};



