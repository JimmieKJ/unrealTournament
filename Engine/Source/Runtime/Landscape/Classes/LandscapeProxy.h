// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LandscapeInfo.h"

#include "Engine/EngineTypes.h"
#include "Engine/Texture.h"
#include "Engine/TextureDefines.h"

#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeGrassType.h"
#include "Tickable.h"
#include "AI/Navigation/NavigationTypes.h"

#include "LandscapeProxy.generated.h"

class ULandscapeMaterialInstanceConstant;
class ULandscapeSplinesComponent;
class ULandscapeHeightfieldCollisionComponent;
class UMaterialInterface;
class UTexture2D;
class ALandscape;
class ALandscapeProxy;
class ULandscapeComponent;
class USplineComponent;

/** Structure storing channel usage for weightmap textures */
USTRUCT()
struct FLandscapeWeightmapUsage
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	ULandscapeComponent* ChannelUsage[4];

	FLandscapeWeightmapUsage()
	{
		ChannelUsage[0] = NULL;
		ChannelUsage[1] = NULL;
		ChannelUsage[2] = NULL;
		ChannelUsage[3] = NULL;
	}
	friend FArchive& operator<<( FArchive& Ar, FLandscapeWeightmapUsage& U );
	int32 FreeChannelCount() const
	{
		return	((ChannelUsage[0] == NULL) ? 1 : 0) + 
				((ChannelUsage[1] == NULL) ? 1 : 0) + 
				((ChannelUsage[2] == NULL) ? 1 : 0) + 
				((ChannelUsage[3] == NULL) ? 1 : 0);
	}
};

USTRUCT()
struct FLandscapeEditorLayerSettings
{
	GENERATED_USTRUCT_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	ULandscapeLayerInfoObject* LayerInfoObj;

	UPROPERTY()
	FString ReimportLayerFilePath;

	FLandscapeEditorLayerSettings()
		: LayerInfoObj(NULL)
		, ReimportLayerFilePath()
	{
	}

	FLandscapeEditorLayerSettings(ULandscapeLayerInfoObject* InLayerInfo, const FString& InFilePath = FString())
		: LayerInfoObj(InLayerInfo)
		, ReimportLayerFilePath(InFilePath)
	{
	}

	bool operator==(const FLandscapeEditorLayerSettings& rhs) const
	{
		return LayerInfoObj == rhs.LayerInfoObj;
	}
#endif // WITH_EDITORONLY_DATA
};

USTRUCT()
struct FLandscapeLayerStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	ULandscapeLayerInfoObject* LayerInfoObj;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	UMaterialInstanceConstant* ThumbnailMIC;

	UPROPERTY()
	ALandscapeProxy* Owner;

	UPROPERTY(transient)
	int32 DebugColorChannel;

	UPROPERTY(transient)
	uint32 bSelected:1;

	UPROPERTY()
	FString SourceFilePath;
#endif // WITH_EDITORONLY_DATA

	FLandscapeLayerStruct()
		: LayerInfoObj(NULL)
#if WITH_EDITORONLY_DATA
		, ThumbnailMIC(NULL)
		, Owner(NULL)
		, DebugColorChannel(0)
		, bSelected(false)
		, SourceFilePath()
#endif // WITH_EDITORONLY_DATA
	{
	}
};

/** Structure storing Layer Data for import */
USTRUCT()
struct FLandscapeImportLayerInfo
{
	GENERATED_USTRUCT_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY(Category="Import", VisibleAnywhere)
	FName LayerName;

	UPROPERTY(Category="Import", EditAnywhere)
	ULandscapeLayerInfoObject* LayerInfo;

	UPROPERTY(Category="Import", VisibleAnywhere)
	UMaterialInstanceConstant* ThumbnailMIC; // Optional

	UPROPERTY(Category="Import", EditAnywhere)
	FString SourceFilePath; // Optional
	
	// Raw weightmap data
	TArray<uint8> LayerData;		
#endif

#if WITH_EDITOR
	FLandscapeImportLayerInfo(FName InLayerName = NAME_None)
	:	LayerName(InLayerName)
	,	LayerInfo(NULL)
	,	ThumbnailMIC(NULL)
	,	SourceFilePath("")
	{
	}

	LANDSCAPE_API FLandscapeImportLayerInfo(const struct FLandscapeInfoLayerSettings& InLayerSettings);
#endif
};

// this is only here because putting it in LandscapeEditorObject.h (where it belongs)
// results in Engine being dependent on LandscapeEditor, as the actual landscape editing
// code (e.g. LandscapeEdit.h) is in /Engine/ for some reason...
UENUM()
namespace ELandscapeLayerPaintingRestriction
{
	enum Type
	{
		// No restriction, can paint anywhere (default)
		None         UMETA(DisplayName="None"),

		// Uses the MaxPaintedLayersPerComponent setting from the LandscapeProxy
		UseMaxLayers UMETA(DisplayName="Limit Layer Count"),

		// Restricts painting to only components that already have this layer
		ExistingOnly UMETA(DisplayName="Existing Layers Only"),
	};
}

UENUM()
namespace ELandscapeLODFalloff
{
	enum Type
	{
		// Default mode
		Linear			UMETA(DisplayName = "Linear"),
		// Square Root give more natural transition, and also keep the same LOD 
		SquareRoot		UMETA(DisplayName = "Square Root"),
	};
}

struct FCachedLandscapeFoliage
{
	struct FGrassCompKey
	{
		TWeakObjectPtr<ULandscapeComponent> BasedOn;
		TWeakObjectPtr<ULandscapeGrassType> GrassType;
		int32 SqrtSubsections;
		int32 CachedMaxInstancesPerComponent;
		int32 SubsectionX;
		int32 SubsectionY;
		int32 NumVarieties;
		int32 VarietyIndex;

		FGrassCompKey()
			: SqrtSubsections(0)
			, CachedMaxInstancesPerComponent(0)
			, SubsectionX(0)
			, SubsectionY(0)
			, NumVarieties(0)
			, VarietyIndex(-1)
		{
		}
		inline bool operator==(const FGrassCompKey& Other) const
		{
			return 
				SqrtSubsections == Other.SqrtSubsections &&
				CachedMaxInstancesPerComponent == Other.CachedMaxInstancesPerComponent &&
				SubsectionX == Other.SubsectionX &&
				SubsectionY == Other.SubsectionY &&
				BasedOn == Other.BasedOn &&
				GrassType == Other.GrassType &&
				NumVarieties == Other.NumVarieties &&
				VarietyIndex == Other.VarietyIndex;
		}

		friend uint32 GetTypeHash(const FGrassCompKey& Key)
		{
			return GetTypeHash(Key.BasedOn) ^ GetTypeHash(Key.GrassType) ^ Key.SqrtSubsections ^ Key.CachedMaxInstancesPerComponent ^ (Key.SubsectionX >> 16) ^ (Key.SubsectionY >> 24) ^ (Key.NumVarieties >> 3) ^ (Key.VarietyIndex >> 13);
		}

	};

	struct FGrassComp
	{
		FGrassCompKey Key;
		TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Foliage;
		uint32 LastUsedFrameNumber;
		double LastUsedTime;
		bool Pending;

		FGrassComp()
			: Pending(true)
		{
			Touch();
		}
		void Touch()
		{
			LastUsedFrameNumber = GFrameNumber;
			LastUsedTime = FPlatformTime::Seconds();
		}
	};

	struct FGrassCompKeyFuncs : BaseKeyFuncs<FGrassComp,FGrassCompKey>
	{
		static KeyInitType GetSetKey(const FGrassComp& Element)
		{
			return Element.Key;
		}

		static bool Matches(KeyInitType A, KeyInitType B)
		{
			return A == B;
		}

		static uint32 GetKeyHash(KeyInitType Key)
		{
			return GetTypeHash(Key);
		}
	};

	typedef TSet<FGrassComp, FGrassCompKeyFuncs> TGrassSet;
	TSet<FGrassComp, FGrassCompKeyFuncs> CachedGrassComps;

	void ClearCache()
	{
		CachedGrassComps.Empty();
	}
};

class FAsyncGrassTask : public FNonAbandonableTask
{
public:
	struct FAsyncGrassBuilder* Builder;
	FCachedLandscapeFoliage::FGrassCompKey Key;
	TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Foliage;

	FAsyncGrassTask(struct FAsyncGrassBuilder* InBuilder, const FCachedLandscapeFoliage::FGrassCompKey& InKey, UHierarchicalInstancedStaticMeshComponent* InFoliage)
		: Builder(InBuilder)
		, Key(InKey)
		, Foliage(InFoliage)
	{
	}
	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncGrassTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	~FAsyncGrassTask();
};

UCLASS(NotPlaceable, NotBlueprintable, hidecategories=(Display, Attachment, Physics, Debug, Lighting, LOD), showcategories=(Lighting, Rendering, "Utilities|Transformation"), MinimalAPI)
class ALandscapeProxy : public AActor, public FTickableGameObject
{
	GENERATED_UCLASS_BODY()

	virtual ~ALandscapeProxy();

	UPROPERTY()
	ULandscapeSplinesComponent* SplineComponent;

protected:
	/** Guid for LandscapeEditorInfo **/
	UPROPERTY()
	FGuid LandscapeGuid;

public:
	/** Offset in quads from global components grid origin (in quads) **/
	UPROPERTY()
	FIntPoint LandscapeSectionOffset;

#if WITH_EDITORONLY_DATA
	/** To support legacy landscape section offset modification under world composition mode */
	UPROPERTY()
	bool bStaticSectionOffset;
#endif

	/** Max LOD level to use when rendering, -1 means the max available */
	UPROPERTY(EditAnywhere, Category=LOD)
	int32 MaxLODLevel;

#if WITH_EDITORONLY_DATA
	/** LOD level to use when exporting the landscape to obj or FBX */
	UPROPERTY(EditAnywhere, Category=LOD)
	int32 ExportLOD;
#endif

	/** LOD level to use when running lightmass (increase to 1 or 2 for large landscapes to stop lightmass crashing) */
	UPROPERTY(EditAnywhere, Category=Lighting)
	int32 StaticLightingLOD;

	/** Default physical material, used when no per-layer values physical materials */
	UPROPERTY(EditAnywhere, Category=Landscape)
	UPhysicalMaterial* DefaultPhysMaterial;

	/**
	 * Allows artists to adjust the distance where textures using UV 0 are streamed in/out.
	 * 1.0 is the default, whereas a higher value increases the streamed-in resolution.
	 * Value can be < 0 (from legcay content, or code changes)
	 */
	UPROPERTY(EditAnywhere, Category=Landscape)
	float StreamingDistanceMultiplier;

	/** Combined material used to render the landscape */
	UPROPERTY(EditAnywhere, Category=Landscape)
	UMaterialInterface* LandscapeMaterial;

	/** Material used to render landscape components with holes. If not set, LandscapeMaterial will be used (blend mode will be overridden to Masked if it is set to Opaque) */
	UPROPERTY(EditAnywhere, Category=Landscape, AdvancedDisplay)
	UMaterialInterface* LandscapeHoleMaterial;

	UPROPERTY(EditAnywhere, Category=LOD)
	float LODDistanceFactor;

	/** The array of LandscapeComponent that are used by the landscape */
	UPROPERTY()
	TArray<ULandscapeComponent*> LandscapeComponents;

	/** Array of LandscapeHeightfieldCollisionComponent */
	UPROPERTY()
	TArray<ULandscapeHeightfieldCollisionComponent*> CollisionComponents;

	UPROPERTY(transient, duplicatetransient)
	TArray<UHierarchicalInstancedStaticMeshComponent*> FoliageComponents;

	/** A transient data structure for tracking the grass */
	FCachedLandscapeFoliage FoliageCache;
	/** A transient data structure for tracking the grass tasks*/
	TArray<FAsyncTask<FAsyncGrassTask>* > AsyncFoliageTasks;

	/**
	 *	The resolution to cache lighting at, in texels/quad in one axis
	 *  Total resolution would be changed by StaticLightingResolution*StaticLightingResolution
	 *	Automatically calculate proper value for removing seams
	 */
	UPROPERTY(EditAnywhere, Category=Lighting)
	float StaticLightingResolution;

	UPROPERTY(EditAnywhere, Category=LandscapeProxy)
	TLazyObjectPtr<ALandscape> LandscapeActor;

	UPROPERTY(EditAnywhere, Category=Lighting, meta=(DisplayName = "Static Shadow"))
	uint32 bCastStaticShadow:1;

	/** Whether this primitive should cast dynamic shadows as if it were a two sided material. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Lighting, meta=(DisplayName = "Shadow Two Sided"))
	uint32 bCastShadowAsTwoSided:1;

	/** Whether this primitive should cast shadows in the far shadow cascades. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Lighting, meta=(DisplayName = "Far Shadow"))
	uint32 bCastFarShadow:1;

	UPROPERTY()
	uint32 bIsProxy:1;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	uint32 bIsMovingToLevel:1;    // Check for the Move to Current Level case
#endif // WITH_EDITORONLY_DATA

	/** The Lightmass settings for this object. */
	UPROPERTY(EditAnywhere, Category=Lightmass)
	struct FLightmassPrimitiveSettings LightmassSettings;

	// Landscape LOD to use for collision tests. Higher numbers use less memory and process faster, but are much less accurate
	UPROPERTY(EditAnywhere, Category=Landscape)
	int32 CollisionMipLevel;

	/** Thickness of the collision surface, in unreal units */
	UPROPERTY(EditAnywhere, Category=Landscape)
	float CollisionThickness;

	/** Collision profile settings for this landscape */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Collision, meta=(ShowOnlyInnerProperties))
	FBodyInstance BodyInstance;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<ULandscapeLayerInfoObject*> EditorCachedLayerInfos_DEPRECATED;

	UPROPERTY()
	FString ReimportHeightmapFilePath;

	UPROPERTY()
	TArray<struct FLandscapeEditorLayerSettings> EditorLayerSettings;
#endif

	/** Data set at creation time */
	UPROPERTY()
	int32 ComponentSizeQuads;    // Total number of quads in each component

	UPROPERTY()
	int32 SubsectionSizeQuads;    // Number of quads for a subsection of a component. SubsectionSizeQuads+1 must be a power of two.

	UPROPERTY()
	int32 NumSubsections;    // Number of subsections in X and Y axis

	/** Hints navigation system whether this landscape will ever be navigated on. true by default, but make sure to set it to false for faraway, background landscapes */
	UPROPERTY(EditAnywhere, Category=Landscape)
	uint32 bUsedForNavigation:1;

	UPROPERTY(EditAnywhere, Category = Landscape, AdvancedDisplay)
	ENavDataGatheringMode NavigationGeometryGatheringMode;

	UPROPERTY(EditAnywhere, Category=LOD)
	TEnumAsByte<enum ELandscapeLODFalloff::Type> LODFalloff;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category=Landscape)
	int32 MaxPaintedLayersPerComponent; // 0 = disabled
#endif

public:

#if WITH_EDITOR
	LANDSCAPE_API static ULandscapeLayerInfoObject* VisibilityLayer;
#endif

	/** Map of material instance constants used to for the components. Key is generated with ULandscapeComponent::GetLayerAllocationKey() */
	TMap<FString, UMaterialInstanceConstant*> MaterialInstanceConstantMap;

	/** Map of weightmap usage */
	TMap<UTexture2D*, struct FLandscapeWeightmapUsage> WeightmapUsageMap;

	// Blueprint functions

	/** Change the Level of Detail distance factor */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	virtual void ChangeLODDistanceFactor(float InLODDistanceFactor);

	// Editor-time blueprint functions

	/** Deform landscape using a given spline
	 * @param StartWidth - Width of the spline at the start node, in Spline Component local space
	 * @param EndWidth   - Width of the spline at the end node, in Spline Component local space
	 * @param StartSideFalloff - Width of the falloff at either side of the spline at the start node, in Spline Component local space
	 * @param EndSideFalloff - Width of the falloff at either side of the spline at the end node, in Spline Component local space
	 * @param StartRoll - Roll applied to the spline at the start node, in degrees. 0 is flat
	 * @param EndRoll - Roll applied to the spline at the end node, in degrees. 0 is flat
	 * @param NumSubdivisions - Number of triangles to place along the spline when applying it to the landscape. Higher numbers give better results, but setting it too high will be slow and may cause artifacts
	 * @param bRaiseHeights - Allow the landscape to be raised up to the level of the spline. If both bRaiseHeights and bLowerHeights are false, no height modification of the landscape will be performed
	 * @param bLowerHeights - Allow the landscape to be lowered down to the level of the spline. If both bRaiseHeights and bLowerHeights are false, no height modification of the landscape will be performed
	 * @param PaintLayer - LayerInfo to paint, or none to skip painting. The landscape must be configured with the same layer info in one of its layers or this will do nothing!
	 */
	UFUNCTION(BlueprintCallable, Category = "Landscape Editor")
	void EditorApplySpline(USplineComponent* InSplineComponent, float StartWidth = 200, float EndWidth = 200, float StartSideFalloff = 200, float EndSideFalloff = 200, float StartRoll = 0, float EndRoll = 0, int32 NumSubdivisions = 20, bool bRaiseHeights = true, bool bLowerHeights = true, ULandscapeLayerInfoObject* PaintLayer = nullptr);

	// End blueprint functions

	// Begin AActor Interface
	virtual void UnregisterAllComponents() override;
	virtual void RegisterAllComponents() override;
	virtual void RerunConstructionScripts() override {}
	virtual bool IsLevelBoundsRelevant() const override { return true; }

#if WITH_EDITOR
	virtual void Destroyed() override;
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	virtual void EditorApplyMirror(const FVector& MirrorScale, const FVector& PivotLocation) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual bool ShouldImport(FString* ActorPropString, bool IsMovingLevel) override;
	virtual bool ShouldExport() override;
	// End AActor Interface
#endif	//WITH_EDITOR

	FGuid GetLandscapeGuid() const { return LandscapeGuid; }
	void SetLandscapeGuid(const FGuid& Guid) { LandscapeGuid = Guid; }
	virtual ALandscape* GetLandscapeActor();

	/* Per-frame call to update dynamic grass placement and render grassmaps */
	void TickGrass();

	/** Flush the grass cache */
	LANDSCAPE_API void FlushGrassComponents(const TSet<ULandscapeComponent*>* OnlyForComponents = nullptr, bool bFlushGrassMaps = true);

	/** 
		Update Grass 
		* @param Cameras to use for culling, if empty, then NO culling
		* @param bForceSync if true, block and finish all work
	*/
	LANDSCAPE_API void UpdateGrass(const TArray<FVector>& Cameras, bool bForceSync = false);

	/* Get the list of grass types on this landscape */
	TArray<ULandscapeGrassType*> GetGrassTypes() const;

	/* Invalidate the precomputed grass and baked texture data for the specified components */
	LANDSCAPE_API static void InvalidateGeneratedComponentData(const TSet<ULandscapeComponent*>& Components);

#if WITH_EDITOR
	/** Render grass maps for the specified components */
	void RenderGrassMaps(const TArray<ULandscapeComponent*>& LandscapeComponents, const TArray<ULandscapeGrassType*>& GrassTypes);

	/** Update any textures baked from the landscape as necessary */
	void UpdateBakedTextures();

	/** Frame counter to count down to the next time we check to update baked textures, so we don't check every frame */
	int32 UpdateBakedTexturesCountdown;
#endif

	// Begin FTickableGameObject interface.
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override 
	{ 
		return !HasAnyFlags(RF_ClassDefaultObject); 
	}
	virtual bool IsTickableWhenPaused() const override
	{
		return !HasAnyFlags(RF_ClassDefaultObject); 
	}
	virtual bool IsTickableInEditor() const override
	{
		return !HasAnyFlags(RF_ClassDefaultObject); 
	}
	virtual TStatId GetStatId() const override
	{
		return GetStatID();
	}

	// Begin UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;
	// End UObject Interface

	LANDSCAPE_API static TArray<FName> GetLayersFromMaterial(UMaterialInterface* Material);
	LANDSCAPE_API TArray<FName> GetLayersFromMaterial() const;
	LANDSCAPE_API static ULandscapeLayerInfoObject* CreateLayerInfo(const TCHAR* LayerName, ULevel* Level);
	LANDSCAPE_API ULandscapeLayerInfoObject* CreateLayerInfo(const TCHAR* LayerName);

	LANDSCAPE_API ULandscapeInfo* GetLandscapeInfo(bool bSpawnNewActor = true);

	// Get Landscape Material assigned to this Landscape
	virtual UMaterialInterface* GetLandscapeMaterial() const;

	// Get Hole Landscape Material assigned to this Landscape
	virtual UMaterialInterface* GetLandscapeHoleMaterial() const;

	// Remove Invalid weightmaps
	LANDSCAPE_API void RemoveInvalidWeightmaps();

	// Changed Physical Material
	LANDSCAPE_API void ChangedPhysMaterial();

	// Check input Landscape actor is match for this LandscapeProxy (by GUID)
	LANDSCAPE_API bool IsValidLandscapeActor(ALandscape* Landscape);

	// Copy properties from parent Landscape actor
	LANDSCAPE_API void GetSharedProperties(ALandscapeProxy* Landscape);
	// Assign only mismatched properties and mark proxy package dirty
	LANDSCAPE_API void ConditionalAssignCommonProperties(ALandscape* Landscape);
	
	/** Get the LandcapeActor-to-world transform with respect to landscape section offset*/
	LANDSCAPE_API FTransform LandscapeActorToWorld() const;
	
	/** Set landscape absolute location in section space */
	LANDSCAPE_API void SetAbsoluteSectionBase(FIntPoint SectionOffset);
	
	/** Get landscape position in section space */
	LANDSCAPE_API FIntPoint GetSectionBaseOffset() const;
	
	/** Recreate all components rendering and collision states */
	LANDSCAPE_API void RecreateComponentsState();

	/** Recreate all collision components based on render component */
	LANDSCAPE_API void RecreateCollisionComponents();

	/** Remove all XYOffset values */
	LANDSCAPE_API void RemoveXYOffsets();


	LANDSCAPE_API static ULandscapeMaterialInstanceConstant* GetLayerThumbnailMIC(UMaterialInterface* LandscapeMaterial, FName LayerName, UTexture2D* ThumbnailWeightmap, UTexture2D* ThumbnailHeightmap, ALandscapeProxy* Proxy);

	LANDSCAPE_API void Import(FGuid Guid, int32 VertsX, int32 VertsY, 
							int32 ComponentSizeQuads, int32 NumSubsections, int32 SubsectionSizeQuads, 
							const uint16* HeightData, const TCHAR* HeightmapFileName, 
							const TArray<FLandscapeImportLayerInfo>& ImportLayerInfos);

	/**
	 * Exports landscape into raw mesh
	 * 
	 * @param InExportLOD Landscape LOD level to use while exporting, INDEX_NONE will use ALanscapeProxy::ExportLOD settings
	 * @param OutRawMesh - Resulting raw mesh
	 * @return true if successful
	 */
	LANDSCAPE_API bool ExportToRawMesh(int32 InExportLOD, struct FRawMesh& OutRawMesh) const;


	/** @return Current size of bounding rectangle in quads space */
	LANDSCAPE_API FIntRect GetBoundingRect() const;

	/** Creates a Texture2D for use by this landscape proxy or one of it's components. If OptionalOverrideOuter is not specified, the level is used. */
	LANDSCAPE_API UTexture2D* CreateLandscapeTexture(int32 InSizeX, int32 InSizeY, TextureGroup InLODGroup, ETextureSourceFormat InFormat, UObject* OptionalOverrideOuter = nullptr) const;

	/* For the grassmap rendering notification */
	int32 NumComponentsNeedingGrassMapRender;
	LANDSCAPE_API static int32 TotalComponentsNeedingGrassMapRender;

	/* To throttle texture streaming when we're trying to render a grassmap */
	int32 NumTexturesToStreamForVisibleGrassMapRender;
	LANDSCAPE_API static int32 TotalTexturesToStreamForVisibleGrassMapRender;

	/* For the texture baking notification */
	int32 NumComponentsNeedingTextureBaking;
	LANDSCAPE_API static int32 TotalComponentsNeedingTextureBaking;

	/** remove an overlapping component. Called from MapCheck. */
	LANDSCAPE_API void RemoveOverlappingComponent(ULandscapeComponent* Component);
#endif
};



