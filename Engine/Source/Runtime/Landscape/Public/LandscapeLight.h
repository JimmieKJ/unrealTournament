// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LandscapeLight.h: Static lighting for LandscapeComponents
=============================================================================*/

#ifndef _LANDSCAPELIGHT_H
#define _LANDSCAPELIGHT_H

#include "StaticLighting.h"

class ULandscapeComponent;

/** A texture mapping for landscapes */
class FLandscapeStaticLightingTextureMapping : public FStaticLightingTextureMapping
{
public:
	/** Initialization constructor. */
	FLandscapeStaticLightingTextureMapping(ULandscapeComponent* InPrimitive,FStaticLightingMesh* InMesh,int32 InLightMapWidth,int32 InLightMapHeight,bool bPerformFullQualityRebuild);

	// FStaticLightingTextureMapping interface
	virtual void Apply(FQuantizedLightmapData* QuantizedData, const TMap<ULightComponent*,FShadowMapData2D*>& ShadowMapData);

#if WITH_EDITOR
	/** 
	* Export static lighting mapping instance data to an exporter 
	* @param Exporter - export interface to process static lighting data
	*/
	UNREALED_API virtual void ExportMapping(class FLightmassExporter* Exporter);
#endif	//WITH_EDITOR

	virtual FString GetDescription() const
	{
		return FString(TEXT("LandscapeMapping"));
	}
private:

	/** The primitive this mapping represents. */
	ULandscapeComponent* const LandscapeComponent;
};



/** Represents the triangles of a Landscape component to the static lighting system. */
class FLandscapeStaticLightingMesh : public FStaticLightingMesh
{
public:
	// tors
	FLandscapeStaticLightingMesh(ULandscapeComponent* InComponent, const TArray<ULightComponent*>& InRelevantLights, int32 InExpandQuadsX, int32 InExpandQuadsY, float LightMapRatio, int32 InLOD);
	virtual ~FLandscapeStaticLightingMesh();

	// FStaticLightingMesh interface
	virtual void GetTriangle(int32 TriangleIndex,FStaticLightingVertex& OutV0,FStaticLightingVertex& OutV1,FStaticLightingVertex& OutV2) const;
	virtual void GetTriangleIndices(int32 TriangleIndex,int32& OutI0,int32& OutI1,int32& OutI2) const;
	virtual FLightRayIntersection IntersectLightRay(const FVector& Start,const FVector& End,bool bFindNearestIntersection) const;

#if WITH_EDITOR
	/** 
	* Export static lighting mesh instance data to an exporter 
	* @param Exporter - export interface to process static lighting data
	**/
	UNREALED_API virtual void ExportMeshInstance(class FLightmassExporter* Exporter) const;
#endif	//WITH_EDITOR

protected:
	void GetHeightmapData(int32 InLOD, int32 GeometryLOD);

	/** Fills in the static lighting vertex data for the Landscape vertex. */
	void GetStaticLightingVertex(int32 VertexIndex, FStaticLightingVertex& OutVertex) const;

	ULandscapeComponent* LandscapeComponent;

	// FLandscapeStaticLightingMeshData
	FTransform LocalToWorld;
	int32 ComponentSizeQuads;
	float LightMapRatio;
	int32 ExpandQuadsX;
	int32 ExpandQuadsY;

	TArray<FColor> HeightData;
	// Cache
	int32 NumVertices;
	int32 NumQuads;
	float UVFactor;
	bool bReverseWinding;

	friend class FLightmassExporter;

#if WITH_EDITOR
public:
	// Cache data for Landscape upscaling data
	LANDSCAPE_API static TMap<FIntPoint, FColor> LandscapeUpscaleHeightDataCache;
	LANDSCAPE_API static TMap<FIntPoint, FColor> LandscapeUpscaleXYOffsetDataCache;
#endif
};

#endif // _LANDSCAPELIGHT_H
