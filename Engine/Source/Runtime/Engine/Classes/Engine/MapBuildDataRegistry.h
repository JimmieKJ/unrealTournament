// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 * Registry for built data from a map build
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Engine/EngineTypes.h"
#include "SceneTypes.h"
#include "UObject/UObjectAnnotation.h"
#include "MapBuildDataRegistry.generated.h"

class FPrecomputedLightVolumeData;

struct ENGINE_API FPerInstanceLightmapData
{
	FVector2D LightmapUVBias;
	FVector2D ShadowmapUVBias;

	FPerInstanceLightmapData()
		: LightmapUVBias(ForceInit)
		, ShadowmapUVBias(ForceInit)
	{}

	friend FArchive& operator<<(FArchive& Ar, FPerInstanceLightmapData& InstanceData)
	{
		// @warning BulkSerialize: FPerInstanceLightmapData is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << InstanceData.LightmapUVBias << InstanceData.ShadowmapUVBias;
		return Ar;
	}
};

class FMeshMapBuildData
{
public:
	FLightMapRef LightMap;
	FShadowMapRef ShadowMap;
	TArray<FGuid> IrrelevantLights;
	TArray<FPerInstanceLightmapData> PerInstanceLightmapData;

	ENGINE_API FMeshMapBuildData();

	/** Destructor. */
	ENGINE_API ~FMeshMapBuildData();

	/**
	 * Determine if this annotation is the default
	 * @return true is this is a default annotation
	 */
	FORCEINLINE bool IsDefault()
	{
		return LightMap == DefaultAnnotation.LightMap && ShadowMap == DefaultAnnotation.ShadowMap;
	}

	ENGINE_API void AddReferencedObjects(FReferenceCollector& Collector);

	/** Serializer. */
	friend ENGINE_API FArchive& operator<<(FArchive& Ar,FMeshMapBuildData& MeshMapBuildData);

	/** Default state for annotations (no flags changed)*/
	static const FMeshMapBuildData DefaultAnnotation;
};

class FMeshMapBuildLegacyData
{
public:

	TArray<TPair<FGuid, FMeshMapBuildData*>> Data;

	/**
	 * Determine if this annotation is the default
	 * @return true is this is a default annotation
	 */
	FORCEINLINE bool IsDefault()
	{
		return Data.Num() == 0;
	}
};

class FStaticShadowDepthMapData
{
public:
	/** Transform from world space to the coordinate space that DepthSamples are stored in. */
	FMatrix WorldToLight;
	/** Dimensions of the generated shadow map. */
	int32 ShadowMapSizeX;
	int32 ShadowMapSizeY;
	/** Shadowmap depth values */
	TArray<FFloat16> DepthSamples;

	FStaticShadowDepthMapData() :
		WorldToLight(FMatrix::Identity),
		ShadowMapSizeX(0),
		ShadowMapSizeY(0)
	{}

	ENGINE_API void Empty();

	friend FArchive& operator<<(FArchive& Ar, FStaticShadowDepthMapData& ShadowMap);
};

class FLevelLegacyMapBuildData
{
public:

	FGuid Id;
	FPrecomputedLightVolumeData* Data;

	FLevelLegacyMapBuildData() :
		Data(NULL)
	{}

	/**
	 * Determine if this annotation is the default
	 * @return true is this is a default annotation
	 */
	FORCEINLINE bool IsDefault()
	{
		return Id == FGuid();
	}
};

class FLightComponentMapBuildData
{
public:

	FLightComponentMapBuildData() :
		ShadowMapChannel(INDEX_NONE)
	{}

	/** 
	 * Shadow map channel which is used to match up with the appropriate static shadowing during a deferred shading pass.
	 * This is generated during a lighting build.
	 */
	int32 ShadowMapChannel;

	FStaticShadowDepthMapData DepthMap;

	friend FArchive& operator<<(FArchive& Ar, FLightComponentMapBuildData& ShadowMap);
};

class FLightComponentLegacyMapBuildData
{
public:

	FGuid Id;
	FLightComponentMapBuildData* Data;

	FLightComponentLegacyMapBuildData() :
		Data(NULL)
	{}

	/**
	 * Determine if this annotation is the default
	 * @return true is this is a default annotation
	 */
	FORCEINLINE bool IsDefault()
	{
		return Id == FGuid();
	}
};

UCLASS(MinimalAPI)
class UMapBuildDataRegistry : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	/** The lighting quality the level was last built with */
	UPROPERTY(Category=Lighting, VisibleAnywhere)
	TEnumAsByte<enum ELightingBuildQuality> LevelLightingQuality;

	//~ Begin UObject Interface
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	ENGINE_API virtual void BeginDestroy() override;
	//~ End UObject Interface

	/** 
	 * Allocates a new FMeshMapBuildData from the registry.
	 * Warning: Further allocations will invalidate the returned reference.
	 */
	ENGINE_API FMeshMapBuildData& AllocateMeshBuildData(const FGuid& MeshId, bool bMarkDirty);
	ENGINE_API const FMeshMapBuildData* GetMeshBuildData(FGuid MeshId) const;
	ENGINE_API FMeshMapBuildData* GetMeshBuildData(FGuid MeshId);

	/** 
	 * Allocates a new FPrecomputedLightVolumeData from the registry.
	 * Warning: Further allocations will invalidate the returned reference.
	 */
	ENGINE_API FPrecomputedLightVolumeData& AllocateLevelBuildData(const FGuid& LevelId);
	ENGINE_API void AddLevelBuildData(const FGuid& LevelId, FPrecomputedLightVolumeData* InData);
	ENGINE_API const FPrecomputedLightVolumeData* GetLevelBuildData(FGuid LevelId) const;
	ENGINE_API FPrecomputedLightVolumeData* GetLevelBuildData(FGuid LevelId);

	/** 
	 * Allocates a new FLightComponentMapBuildData from the registry.
	 * Warning: Further allocations will invalidate the returned reference.
	 */
	ENGINE_API FLightComponentMapBuildData& FindOrAllocateLightBuildData(FGuid LightId, bool bMarkDirty);
	ENGINE_API const FLightComponentMapBuildData* GetLightBuildData(FGuid LightId) const;
	ENGINE_API FLightComponentMapBuildData* GetLightBuildData(FGuid LightId);

	ENGINE_API void InvalidateStaticLighting(UWorld* World);

	ENGINE_API bool IsLegacyBuildData() const;

private:

	ENGINE_API void EmptyData();

	TMap<FGuid, FMeshMapBuildData> MeshBuildData;
	TMap<FGuid, FPrecomputedLightVolumeData*> LevelBuildData;
	TMap<FGuid, FLightComponentMapBuildData> LightBuildData;
};

extern ENGINE_API FUObjectAnnotationSparse<FMeshMapBuildLegacyData, true> GComponentsWithLegacyLightmaps;
extern ENGINE_API FUObjectAnnotationSparse<FLevelLegacyMapBuildData, true> GLevelsWithLegacyBuildData;
extern ENGINE_API FUObjectAnnotationSparse<FLightComponentLegacyMapBuildData, true> GLightComponentsWithLegacyBuildData;
