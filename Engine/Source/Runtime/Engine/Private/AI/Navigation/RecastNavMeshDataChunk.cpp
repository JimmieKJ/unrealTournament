// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/Navigation/RecastNavMeshDataChunk.h"
#include "AI/Navigation/PImplRecastNavMesh.h"
#include "AI/Navigation/RecastHelpers.h"

//----------------------------------------------------------------------//
// FRecastTileData                                                                
//----------------------------------------------------------------------//
FRecastTileData::FRawData::FRawData(uint8* InData)
	: RawData(InData)
{
}

FRecastTileData::FRawData::~FRawData()
{
#if WITH_RECAST
	dtFree(RawData);
#else
	FMemory::Free(RawData);
#endif
}

FRecastTileData::FRecastTileData()
	: TileRef(0)
	, TileDataSize(0)
	, TileCacheDataSize(0)
{
}

FRecastTileData::FRecastTileData(NavNodeRef Ref, int32 DataSize, uint8* RawData, int32 CacheDataSize, uint8* CacheRawData)
	: TileRef(Ref)
	, TileDataSize(DataSize)
	, TileCacheDataSize(CacheDataSize)
{
	TileRawData = MakeShareable(new FRawData(RawData));
	TileCacheRawData = MakeShareable(new FRawData(CacheRawData));
}

// Helper to duplicate recast raw data
static uint8* DuplicateRecastRawData(uint8* Src, int32 SrcSize)
{
#if WITH_RECAST	
	uint8* DupData = (uint8*)dtAlloc(SrcSize, DT_ALLOC_PERM);
#else
	uint8* DupData = (uint8*)FMemory::Malloc(SrcSize);
#endif
	FMemory::Memcpy(DupData, Src, SrcSize);
	return DupData;
}

//----------------------------------------------------------------------//
// URecastNavMeshDataChunk                                                                
//----------------------------------------------------------------------//
URecastNavMeshDataChunk::URecastNavMeshDataChunk(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URecastNavMeshDataChunk::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	int32 NavMeshVersion = NAVMESHVER_LATEST;
	Ar << NavMeshVersion;

	// when writing, write a zero here for now.  will come back and fill it in later.
	int64 RecastNavMeshSizeBytes = 0;
	int64 RecastNavMeshSizePos = Ar.Tell();
	Ar << RecastNavMeshSizeBytes;

	if (Ar.IsLoading())
	{
		if (NavMeshVersion < NAVMESHVER_MIN_COMPATIBLE)
		{
			// incompatible, just skip over this data.  navmesh needs rebuilt.
			Ar.Seek(RecastNavMeshSizePos + RecastNavMeshSizeBytes);
		}
#if WITH_RECAST
		else if (RecastNavMeshSizeBytes > 4)
		{
			SerializeRecastData(Ar, NavMeshVersion);
		}
#endif// WITH_RECAST
		else
		{
			// empty, just skip over this data
			Ar.Seek(RecastNavMeshSizePos + RecastNavMeshSizeBytes);
		}
	}
	else if (Ar.IsSaving())
	{
#if WITH_RECAST
		SerializeRecastData(Ar, NavMeshVersion);
#endif// WITH_RECAST

		int64 CurPos = Ar.Tell();
		RecastNavMeshSizeBytes = CurPos - RecastNavMeshSizePos;
		Ar.Seek(RecastNavMeshSizePos);
		Ar << RecastNavMeshSizeBytes;
		Ar.Seek(CurPos);
	}
}

#if WITH_RECAST
void URecastNavMeshDataChunk::SerializeRecastData(FArchive& Ar, int32 NavMeshVersion)
{
	int32 TileNum = Tiles.Num();
	Ar << TileNum;

	if (Ar.IsLoading())
	{
		Tiles.Empty(TileNum);
		for (int32 TileIdx = 0; TileIdx < TileNum; TileIdx++)
		{
			int32 TileDataSize = 0;
			Ar << TileDataSize;

			// Load tile data 
			uint8* TileRawData = nullptr;
			FPImplRecastNavMesh::SerializeRecastMeshTile(Ar, NavMeshVersion, TileRawData, TileDataSize);
			
			if (TileRawData != nullptr)
			{
				// Load compressed tile cache layer
				int32 TileCacheDataSize = 0;
				uint8* TileCacheRawData = nullptr;
				if (Ar.UE4Ver() >= VER_UE4_ADD_MODIFIERS_RUNTIME_GENERATION && 
					(Ar.EngineVer().GetMajor() != 4 || Ar.EngineVer().GetMinor() != 7)) // Merged package from 4.7 branch
				{
					FPImplRecastNavMesh::SerializeCompressedTileCacheData(Ar, NavMeshVersion, TileCacheRawData, TileCacheDataSize);
				}
				
				// We are owner of tile raw data
				FRecastTileData TileData(0, TileDataSize, TileRawData, TileCacheDataSize, TileCacheRawData);
				Tiles.Add(TileData);
			}
		}
	}
	else if (Ar.IsSaving())
	{
		for (FRecastTileData& TileData : Tiles)
		{
			if (TileData.TileRawData.IsValid())
			{
				// Save tile itself
				Ar << TileData.TileDataSize;
				FPImplRecastNavMesh::SerializeRecastMeshTile(Ar, NavMeshVersion, TileData.TileRawData->RawData, TileData.TileDataSize);
				// Save compressed tile cache layer
				FPImplRecastNavMesh::SerializeCompressedTileCacheData(Ar, NavMeshVersion, TileData.TileCacheRawData->RawData, TileData.TileCacheDataSize);
			}
		}
	}
}
#endif// WITH_RECAST

TArray<uint32> URecastNavMeshDataChunk::AttachTiles(FPImplRecastNavMesh& NavMeshImpl)
{
	TArray<uint32> Result;
	Result.Reserve(Tiles.Num());

#if WITH_RECAST	
	check(NavMeshImpl.NavMeshOwner && NavMeshImpl.NavMeshOwner->GetWorld());
	const bool bIsGame = NavMeshImpl.NavMeshOwner->GetWorld()->IsGameWorld();
	dtNavMesh* NavMesh = NavMeshImpl.DetourNavMesh;

	if (NavMesh != nullptr)
	{
		for (FRecastTileData& TileData : Tiles)
		{
			if (TileData.TileRawData.IsValid())
			{
				// Attach mesh tile to target nav mesh 
				dtStatus status = NavMesh->addTile(TileData.TileRawData->RawData, TileData.TileDataSize, DT_TILE_FREE_DATA, 0, &TileData.TileRef);
				if (dtStatusFailed(status))
				{
					TileData.TileRef = 0;
					continue;
				}

				if (bIsGame)
				{
					// We don't own tile data anymore it will be released by recast navmesh 
					TileData.TileDataSize = 0;
					TileData.TileRawData->RawData = nullptr;
				}
				else
				{
					// In the editor we still need to own data, so make a copy of it
					TileData.TileRawData->RawData = DuplicateRecastRawData(TileData.TileRawData->RawData, TileData.TileDataSize);
				}

				// Attach tile cache layer to target nav mesh
				if (TileData.TileCacheDataSize > 0)
				{
					const dtMeshTile* MeshTile = NavMesh->getTileByRef(TileData.TileRef);
					check(MeshTile);
					int32 TileX = MeshTile->header->x;
					int32 TileY = MeshTile->header->y;
					int32 TileLayerIdx = MeshTile->header->layer;
					FBox TileBBox = Recast2UnrealBox(MeshTile->header->bmin, MeshTile->header->bmax);

					FNavMeshTileData LayerData(TileData.TileCacheRawData->RawData, TileData.TileCacheDataSize, TileLayerIdx, TileBBox);
					NavMeshImpl.AddTileCacheLayer(TileX, TileY, TileLayerIdx, LayerData);

					if (bIsGame)
					{
						// We don't own tile cache data anymore it will be released by navmesh
						TileData.TileCacheDataSize = 0;
						TileData.TileCacheRawData->RawData = nullptr;
					}
					else
					{
						// In the editor we still need to own data, so make a copy of it
						TileData.TileCacheRawData->RawData = DuplicateRecastRawData(TileData.TileCacheRawData->RawData, TileData.TileCacheDataSize);
					}
				}

				Result.Add(NavMesh->decodePolyIdTile(TileData.TileRef));
			}
		}
	}
#endif// WITH_RECAST

	UE_LOG(LogNavigation, Log, TEXT("Attached %d tiles to NavMesh - %s"), Result.Num(), *NavigationDataName.ToString());
	return Result;
}

TArray<uint32> URecastNavMeshDataChunk::DetachTiles(FPImplRecastNavMesh& NavMeshImpl)
{
	TArray<uint32> Result;
	Result.Reserve(Tiles.Num());

#if WITH_RECAST
	check(NavMeshImpl.NavMeshOwner && NavMeshImpl.NavMeshOwner->GetWorld());
	const bool bIsGame = NavMeshImpl.NavMeshOwner->GetWorld()->IsGameWorld();
	dtNavMesh* NavMesh = NavMeshImpl.DetourNavMesh;

	if (NavMesh != nullptr)
	{
		for (FRecastTileData& TileData : Tiles)
		{
			if (TileData.TileRef != 0)
			{
				// Detach tile cache layer and take ownership over compressed data
				const dtMeshTile* MeshTile = NavMesh->getTileByRef(TileData.TileRef);
				if (MeshTile)
				{
					if (bIsGame)
					{
						FNavMeshTileData TileCacheData = NavMeshImpl.GetTileCacheLayer(MeshTile->header->x, MeshTile->header->y, MeshTile->header->layer);
						if (TileCacheData.IsValid())
						{
							TileData.TileCacheDataSize = TileCacheData.DataSize;
							TileData.TileCacheRawData->RawData = TileCacheData.Release();
						}
					}
				
					NavMeshImpl.RemoveTileCacheLayer(MeshTile->header->x, MeshTile->header->y, MeshTile->header->layer);
				}
						
				if (bIsGame)
				{
					// Remove tile from navmesh and take ownership of tile raw data
					NavMesh->removeTile(TileData.TileRef, &TileData.TileRawData->RawData, &TileData.TileDataSize);
				}
				else
				{
					// In the editor we have a copy of tile data so just release tile in navmesh
					NavMesh->removeTile(TileData.TileRef, nullptr, nullptr);
				}
						
				Result.Add(NavMesh->decodePolyIdTile(TileData.TileRef));
			}
		}
	}
#endif// WITH_RECAST

	UE_LOG(LogNavigation, Log, TEXT("Detached %d tiles from NavMesh - %s"), Result.Num(), *NavigationDataName.ToString());
	return Result;
}

int32 URecastNavMeshDataChunk::GetNumTiles() const
{
	return Tiles.Num();
}

void URecastNavMeshDataChunk::ReleaseTiles()
{
	Tiles.Reset();
}

#if WITH_EDITOR
void URecastNavMeshDataChunk::GatherTiles(const FPImplRecastNavMesh* NavMeshImpl, const TArray<int32>& TileIndices)
{
	Tiles.Empty(TileIndices.Num());

	const dtNavMesh* NavMesh = NavMeshImpl->DetourNavMesh;
	
	for (int32 TileIdx : TileIndices)
	{
		const dtMeshTile* Tile = NavMesh->getTile(TileIdx);
		if (Tile && Tile->header)
		{
			// Make our own copy of tile data
			uint8* RawTileData = DuplicateRecastRawData(Tile->data, Tile->dataSize);

			// We need tile cache data only if navmesh supports any kind of runtime generation
			FNavMeshTileData TileCacheData;
			uint8* RawTileCacheData = nullptr;
			if (NavMeshImpl->NavMeshOwner->SupportsRuntimeGeneration())
			{
				TileCacheData = NavMeshImpl->GetTileCacheLayer(Tile->header->x, Tile->header->y, Tile->header->layer);
				if (TileCacheData.IsValid())
				{
					// Make our own copy of tile cache data
					RawTileCacheData = DuplicateRecastRawData(TileCacheData.GetData(), TileCacheData.DataSize);
				}
			}

			FRecastTileData RecastTileData(NavMesh->getTileRef(Tile), Tile->dataSize, RawTileData, TileCacheData.DataSize, RawTileCacheData);
			Tiles.Add(RecastTileData);
		}
	}
}
#endif// WITH_EDITOR

//----------------------------------------------------------------------//
// DEPRECATED
//----------------------------------------------------------------------//
TArray<uint32> URecastNavMeshDataChunk::AttachTiles(FPImplRecastNavMesh* NavMeshImpl)
{
	if (NavMeshImpl)
	{
		return AttachTiles(*NavMeshImpl);
	}

	return TArray<uint32>();
}

TArray<uint32> URecastNavMeshDataChunk::DetachTiles(FPImplRecastNavMesh* NavMeshImpl)
{
	if (NavMeshImpl)
	{
		return DetachTiles(*NavMeshImpl);
	}

	return TArray<uint32>();
}
