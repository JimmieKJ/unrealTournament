// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/VertexAnim/VertexAnimBase.h"
#include "MorphTarget.generated.h"

class USkeletalMesh;

/**
* Morph mesh vertex data used for comparisons and importing
*/
struct FMorphMeshVertexRaw
{
	FVector Position;
	FVector TanX,TanY,TanZ;
};

/**
* Converts a mesh to raw vertex data used to generate a morph target mesh
*/
class FMorphMeshRawSource
{
public:
	/** vertex data used for comparisons */
	TArray<FMorphMeshVertexRaw> Vertices;
	/** index buffer used for comparison */
	TArray<uint32> Indices;
	/** indices to original imported wedge points */
	TArray<uint32> WedgePointIndices;
	/** mesh that provided the source data */
	UObject* SourceMesh;

	/** Constructor (default) */
	ENGINE_API FMorphMeshRawSource() : 
	SourceMesh(NULL)
	{
	}

	ENGINE_API FMorphMeshRawSource( USkeletalMesh* SrcMesh, int32 LODIndex=0 );
	ENGINE_API FMorphMeshRawSource( UStaticMesh* SrcMesh, int32 LODIndex=0 );

	ENGINE_API bool IsValidTarget( const FMorphMeshRawSource& Target ) const;
};



/**
* Mesh data for a single LOD model of a morph target
*/
struct FMorphTargetLODModel
{
	/** vertex data for a single LOD morph mesh */
	TArray<FVertexAnimDelta> Vertices;
	/** number of original verts in the base mesh */
	int32 NumBaseMeshVerts;

	/** pipe operator */
	friend FArchive& operator<<( FArchive& Ar, FMorphTargetLODModel& M )
	{
		Ar << M.Vertices << M.NumBaseMeshVerts;
		return Ar;
	}
};


UCLASS(hidecategories=Object, MinimalAPI)
class UMorphTarget : public UVertexAnimBase
{
	GENERATED_UCLASS_BODY()

public:
	/** morph mesh vertex data for each LOD */
	TArray<FMorphTargetLODModel>	MorphLODModels;

	// Begin UObject interface.
	virtual void Serialize( FArchive& Ar ) override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	// Begin UObject interface.

	/** Post process after importing **/
	ENGINE_API void PostProcess( USkeletalMesh * NewMesh, const FMorphMeshRawSource& BaseSource, const FMorphMeshRawSource& TargetSource, int32 LODIndex, bool bCompareNormal );

	/** Remap vertex indices with base mesh. */
	void RemapVertexIndices( USkeletalMesh* InBaseMesh, const TArray< TArray<uint32> > & BasedWedgePointIndices );

	// Begin UVertexAnimBase interface
	virtual FVertexAnimDelta* GetDeltasAtTime(float Time, int32 LODIndex, FVertexAnimEvalStateBase* State, int32& OutNumDeltas) override;
	virtual bool HasDataForLOD(int32 LODIndex) override;
	// End UVertexAnimBase interface

private:
	/**
	* Generate the streams for this morph target mesh using
	* a base mesh and a target mesh to find the positon differences
	* and other vertex attributes.
	*
	* @param	BaseSource - source mesh for comparing position differences
	* @param	TargetSource - final target vertex positions/attributes 
	* @param	LODIndex - level of detail to use for the geometry
	*/
	void CreateMorphMeshStreams( const FMorphMeshRawSource& BaseSource, const FMorphMeshRawSource& TargetSource, int32 LODIndex, bool bCompareNormal );
};

