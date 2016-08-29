// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	FRecastTileData(NavNodeRef Ref, int32 TileDataSize, uint8* TileRawData, int32 TileCacheDataSize, uint8* TileCacheRawData);
	
	NavNodeRef				TileRef;

	// Tile data
	int32					TileDataSize;
	TSharedPtr<FRawData>	TileRawData;

	// Compressed tile cache layer 
	int32					TileCacheDataSize;
	TSharedPtr<FRawData>	TileCacheRawData;
};

class dtNavMesh;
class FPImplRecastNavMesh;

/** 
 * 
 */
UCLASS()
class ENGINE_API URecastNavMeshDataChunk : public UNavigationDataChunk
{
	GENERATED_UCLASS_BODY()

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	/** Attaches tiles to specified navmesh, transferring tile ownership to navmesh */
	TArray<uint32> AttachTiles(FPImplRecastNavMesh& NavMeshImpl);
	
	/** Detaches tiles from specified navmesh, taking tile ownership */
	TArray<uint32> DetachTiles(FPImplRecastNavMesh& NavMeshImpl);
	
	/** Number of tiles in this chunk */
	int32 GetNumTiles() const;

	/** Releases all tiles that this chunk holds */
	void ReleaseTiles();

public:
#if WITH_EDITOR
	void GatherTiles(const FPImplRecastNavMesh* NavMeshImpl, const TArray<int32>& TileIndices);
#endif// WITH_EDITOR

private:
#if WITH_RECAST
	void SerializeRecastData(FArchive& Ar, int32 NavMeshVersion);
#endif//WITH_RECAST

	
	DEPRECATED(4.8, "AttachTiles is deprecated. Use the version takin a reference instead.")
	TArray<uint32> AttachTiles(FPImplRecastNavMesh* NavMeshImpl);
	DEPRECATED(4.8, "DetachTiles is deprecated. Use the version takin a reference instead.")
	TArray<uint32> DetachTiles(FPImplRecastNavMesh* NavMeshImpl);

private:
	TArray<FRecastTileData> Tiles;
};
