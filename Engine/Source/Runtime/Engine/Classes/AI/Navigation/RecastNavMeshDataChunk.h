// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NavigationDataChunk.h"
#include "RecastNavMeshDataChunk.generated.h"

struct FRecastTileData
{
	struct FRawData
	{
		FRawData(uint8* InData);
		~FRawData();

		uint8* RawData;
	};

	FRecastTileData();
	FRecastTileData(NavNodeRef Ref, int32 DataSize, uint8* RawData);
	
	NavNodeRef				TileRef;
	int32					TileDataSize;
	TSharedPtr<FRawData>	TileRawData;
};

class dtNavMesh;

/** 
 * 
 */
UCLASS()
class ENGINE_API URecastNavMeshDataChunk : public UNavigationDataChunk
{
	GENERATED_UCLASS_BODY()

	// Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	// End UObject Interface

	/** Attaches tiles to specified navmesh, transferring tile ownership to navmesh */
	TArray<uint32> AttachTiles(dtNavMesh* NavMesh);
	
	/** Detaches tiles from specified navmesh, taking tile ownership */
	TArray<uint32> DetachTiles(dtNavMesh* NavMesh);
	
	/** Number of tiles in this chunk */
	int32 GetNumTiles() const;

	/** Releases all tiles that this chunk holds */
	void ReleaseTiles();

public:
#if WITH_EDITOR
	void GatherTiles(const dtNavMesh* NavMesh, const TArray<int32>& TileIndices);
#endif// WITH_EDITOR

private:
#if WITH_RECAST
	void SerializeRecastData(FArchive& Ar);
#endif//WITH_RECAST

private:
	TArray<FRecastTileData> Tiles;
};
