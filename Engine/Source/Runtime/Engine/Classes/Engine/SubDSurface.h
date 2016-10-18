// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "../Curves/CurveFloat.h"
#include "SubDSurface.generated.h"

struct FSubDMeshSection
{
	/** Start face (n-gon) index where this section starts  */
	uint32 FirstIndex;
	/** Number of faces (n-gons) for this section */
	uint32 FaceCount;

	/** The index of the material with which to render this section. 65k should be enough */
	uint16 MaterialIndex;

	/** If true, collision is enabled for this section. */
	uint32 bEnableCollision:1;
	/** If true, this section will cast a shadow. */
	uint32 bCastShadow:1;

	/** Constructor. */
	FSubDMeshSection()
		: FirstIndex(0)
		, FaceCount(0)
		, MaterialIndex(0)
		, bEnableCollision(false)
		, bCastShadow(true)
	{}

	/** Serializer. */
//	friend FArchive& operator<<(FArchive& Ar,FSubDMeshSection& Section);
};

// typical vertex attribute formats but as they are only CPU processed at the moment we could chnage that
UENUM()
enum EVertexAttributeStreamType
{
	VAST_unknown,
	VAST_float,
	VAST_float2,		// FVector2D e.g. UV
	VAST_float3,		// FVector e.g. position
	VAST_float4,
//	VAST_byteARGB,
};

UCLASS(MinimalAPI)
class UVertexAttributeStream : public UObject
{
	GENERATED_UCLASS_BODY()

	// @return same as what GetFVector() would return
	ENGINE_API FVector* CreateFVectorUninitialized(uint32 Count);
	// @return same as what GetFVector2D() would return
	ENGINE_API FVector2D* CreateFVector2DUninitialized(uint32 Count);

	// @return 0 if there is no data or it's not the right type
	ENGINE_API FVector* GetFVector(uint32& OutCount);

	// @return 0 if there is no data or it's not the right type
	ENGINE_API FVector2D* GetFVector2D(uint32& OutCount);

	ENGINE_API uint32 Num() const;
	
	// --------------------------------------------------------

	// e.g. FName(TEXT("Position"))
	UPROPERTY()
	FName Usage;

private:

	// e.g. VAST_unknown
	UPROPERTY()
	TEnumAsByte<enum EVertexAttributeStreamType> AttributeType;

	// actual type depends on AttributeType, position is indexed by IndicesPerFace, other attributes by CornerID (e.g. 10 quads result in 40 attributes)
	UPROPERTY()
	TArray<uint8> Data;

	ENGINE_API uint32 GetTypeSize() const;
};

/**
 * Subdivision Surface Asset (Experimental, Early work in progress)
 */
UCLASS(autoexpandcategories = SubDSurface, MinimalAPI)
class USubDSurface : public UObject
{
	GENERATED_UCLASS_BODY()

	// becomes TopologyDescriptor::numVertsPerFace, TopologyDescriptor::numFaces = .Num()
	// 32bit for OpenSubDiv TopologyDescriptor but it could be limited to 4 bits
	UPROPERTY()
	TArray<uint32> VertexCountPerFace;

	// becomes TopologyDescriptor::vertIndicesPerFace e.g. two triangles: 0,1,2, 2,3,0
	UPROPERTY()
	TArray<uint32> IndicesPerFace;

	// various streams e.g "Position"
	UPROPERTY()
	TArray<UVertexAttributeStream*> VertexAttributeStreams;
	
	TArray<FSubDMeshSection> Sections;

	/** Materials used by this mesh. Individual sections index in to this array. */
	UPROPERTY()
	TArray<UMaterialInterface*> Materials;

	// --------------------------

	// todo
	ENGINE_API uint32 GetVertexCount() const;

	// @return 0 if not found
	ENGINE_API UVertexAttributeStream* CreateVertexAttributeStream(FName InUsage);

	// @return 0 if not found
	ENGINE_API UVertexAttributeStream* FindStreamByUsage(FName InUsage);

/* later
    int           numCreases;
    Index const * creaseVertexIndexPairs;
    float const * creaseWeights;

    int           numCorners;
    Index const * cornerVertexIndices;
    float const * cornerWeights;
*/

	/** A fence which is used to keep track of the rendering thread releasing the mesh resources. */
//	FRenderCommandFence ReleaseResourcesFence;
};

