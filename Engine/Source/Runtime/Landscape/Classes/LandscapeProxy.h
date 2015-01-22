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

#include "LandscapeProxy.generated.h"

class ULandscapeMaterialInstanceConstant;
class ULandscapeLayerInfoObject;
class ULandscapeSplinesComponent;
class ULandscapeHeightfieldCollisionComponent;
class UMaterialInterface;
class UTexture2D;
class ALandscape;
class ALandscapeProxy;
class ULandscapeComponent;

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

UCLASS(NotPlaceable, hidecategories=(Display, Attachment, Physics, Debug, Lighting, LOD), showcategories=(Rendering, "Utilities|Transformation"), MinimalAPI)
class ALandscapeProxy : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	ULandscapeSplinesComponent* SplineComponent;

protected:
	/** Guid for LandscapeEditorInfo **/
	UPROPERTY()
	FGuid LandscapeGuid;

public:
	/** Offset in quads from landscape actor origin **/
	UPROPERTY()
	FIntPoint LandscapeSectionOffset;

	/** Max LOD level to use when rendering */
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
	 */
	UPROPERTY(EditAnywhere, Category=Landscape, meta=(ClampMin=0))
	float StreamingDistanceMultiplier;

	/** Combined material used to render the landscape */
	UPROPERTY(EditAnywhere, Category=Landscape)
	UMaterialInterface* LandscapeMaterial;

	/** Material used to render landscape components with holes. Should be a BLEND_Masked version of LandscapeMaterial. */
	UPROPERTY(EditAnywhere, Category=Landscape)
	UMaterialInterface* LandscapeHoleMaterial;

	UPROPERTY(EditAnywhere, Category=LOD)
	float LODDistanceFactor;

	/** The array of LandscapeComponent that are used by the landscape */
	UPROPERTY()
	TArray<ULandscapeComponent*> LandscapeComponents;

	/** Array of LandscapeHeightfieldCollisionComponent */
	UPROPERTY()
	TArray<ULandscapeHeightfieldCollisionComponent*> CollisionComponents;

	/**
	 *	The resolution to cache lighting at, in texels/quad in one axis
	 *  Total resolution would be changed by StaticLightingResolution*StaticLightingResolution
	 *	Automatically calculate proper value for removing seams
	 */
	UPROPERTY(EditAnywhere, Category=Lighting)
	float StaticLightingResolution;

	UPROPERTY(EditAnywhere, Category=LandscapeProxy)
	TLazyObjectPtr<ALandscape> LandscapeActor;

	UPROPERTY(EditAnywhere, Category=Lighting)
	uint32 bCastStaticShadow:1;

	/** Whether this primitive should cast dynamic shadows as if it were a two sided material. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Lighting)
	uint32 bCastShadowAsTwoSided:1;

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

	/** Change the Level of Detail distance factor */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	virtual void ChangeLODDistanceFactor(float InLODDistanceFactor);

	/** Hints navigation system whether this landscape will ever be navigated on. true by default, but make sure to set it to false for faraway, background landscapes */
	UPROPERTY(EditAnywhere, Category=Landscape)
	uint32 bUsedForNavigation:1;

	UPROPERTY(EditAnywhere, Category=LOD)
	TEnumAsByte<enum ELandscapeLODFalloff::Type> LODFalloff;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category=Landscape)
	int32 MaxPaintedLayersPerComponent; // 0 = disabled
#endif

public:
	LANDSCAPE_API static ULandscapeLayerInfoObject* VisibilityLayer;

	/** Map of material instance constants used to for the components. Key is generated with ULandscapeComponent::GetLayerAllocationKey() */
	TMap<FString, UMaterialInstanceConstant*> MaterialInstanceConstantMap;

	/** Map of weightmap usage */
	TMap<UTexture2D*, struct FLandscapeWeightmapUsage> WeightmapUsageMap;

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
	virtual bool GetSelectedComponents(TArray<UObject*>& SelectedObjects) override;
	// End AActor Interface
#endif	//WITH_EDITOR

	FGuid GetLandscapeGuid() const { return LandscapeGuid; }
	virtual ALandscape* GetLandscapeActor();

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
#endif
};



