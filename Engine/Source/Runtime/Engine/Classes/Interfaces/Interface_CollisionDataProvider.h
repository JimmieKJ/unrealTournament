// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Interface for objects that have a PhysX collision representation and need their geometry cooked
 *
 */

#pragma once
#include "Interface_CollisionDataProvider.generated.h"

// Vertex indices necessary to describe the vertices listed in TriMeshCollisionData
USTRUCT()
struct FTriIndices
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 v0;

	UPROPERTY()
	int32 v1;

	UPROPERTY()
	int32 v2;


	FTriIndices()
		: v0(0)
		, v1(0)
		, v2(0)
	{
	}

};

// Description of triangle mesh collision data necessary for cooking physics data
USTRUCT()
struct FTriMeshCollisionData
{
	GENERATED_USTRUCT_BODY()

	/** Array of vertices included in the triangle mesh */
	UPROPERTY(transient)
	TArray<FVector> Vertices;

	/** Array of indices defining the ordering of triangles in the mesh */
	UPROPERTY(transient)
	TArray<struct FTriIndices> Indices;

	/** Does the mesh require its normals flipped (see PxMeshFlag) */
	UPROPERTY(transient)
	uint32 bFlipNormals:1;

	FTriMeshCollisionData()
		: bFlipNormals(false)
	{
	}


	/** Array of optional material indices (must equal num triangles) */
	TArray<uint16>	MaterialIndices;
};

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class UInterface_CollisionDataProvider : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IInterface_CollisionDataProvider
{
	GENERATED_IINTERFACE_BODY()


	/**	 Interface for retrieving triangle mesh collision data from the implementing object 
	 *
	 * @param CollisionData - structure given by the caller to be filled with tri mesh collision data
	 * @return true if successful, false if unable to successfully fill in data structure
	 */
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) { return false; }

	/**	 Interface for checking if the implementing objects contains triangle mesh collision data 
	 *
	 * @return true if the implementing object contains triangle mesh data, false otherwise
	 */
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const { return false; }

	/** Do we want to create a negative version of this mesh */
	virtual bool WantsNegXTriMesh() { return false; }

	/** An optional string identifying the mesh data. */
	virtual void GetMeshId(FString& OutMeshId) {}
};

