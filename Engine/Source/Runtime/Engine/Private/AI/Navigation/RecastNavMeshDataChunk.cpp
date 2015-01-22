// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/Navigation/RecastNavMeshDataChunk.h"
#include "AI/Navigation/PImplRecastNavMesh.h"

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
	delete RawData;
#endif
}

FRecastTileData::FRecastTileData()
	: TileRef(0)
	, TileDataSize(0)
{
}
	
FRecastTileData::FRecastTileData(NavNodeRef Ref, int32 DataSize, uint8* RawData)
	: TileRef(Ref)
	, TileDataSize(DataSize)
{
	TileRawData = MakeShareable(new FRawData(RawData));
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
			SerializeRecastData(Ar);
		}
#endif// WITH_RECAST
		else
		{
			// empty, just skip over this data
			Ar.Seek(RecastNavMeshSizePos + RecastNavMeshSizeBytes);
		}
	}
	else
	{
#if WITH_RECAST
		SerializeRecastData(Ar);
#endif// WITH_RECAST

		if (Ar.IsSaving())
		{
			int64 CurPos = Ar.Tell();
			RecastNavMeshSizeBytes = CurPos - RecastNavMeshSizePos;
			Ar.Seek(RecastNavMeshSizePos);
			Ar << RecastNavMeshSizeBytes;
			Ar.Seek(CurPos);
		}
	}
}

#if WITH_RECAST
void URecastNavMeshDataChunk::SerializeRecastData(FArchive& Ar)
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

			uint8* TileRawData = nullptr;
			FPImplRecastNavMesh::SerializeRecastMeshTile(Ar, TileRawData, TileDataSize);
			
			if (TileRawData != nullptr)
			{
				FRecastTileData TileData(0, TileDataSize, TileRawData);
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
				Ar << TileData.TileDataSize;
				FPImplRecastNavMesh::SerializeRecastMeshTile(Ar, TileData.TileRawData->RawData, TileData.TileDataSize);
			}
		}
	}
}
#endif// WITH_RECAST

TArray<uint32> URecastNavMeshDataChunk::AttachTiles(dtNavMesh* NavMesh)
{
	TArray<uint32> Result;
	Result.Reserve(Tiles.Num());

#if WITH_RECAST	
	for (FRecastTileData& TileData : Tiles)
	{
		if (TileData.TileRawData.IsValid())
		{
			dtStatus status = NavMesh->addTile(TileData.TileRawData->RawData, TileData.TileDataSize, DT_TILE_FREE_DATA, 0, &TileData.TileRef);
			if (dtStatusFailed(status))
			{
				TileData.TileRef = 0;
				continue;
			}
			
			// We don't own data anymore
			TileData.TileDataSize = 0;
			TileData.TileRawData->RawData = nullptr;

			Result.Add(NavMesh->decodePolyIdTile(TileData.TileRef));
		}
	}
#endif// WITH_RECAST

	UE_LOG(LogNavigation, Log, TEXT("Attached %d tiles to NavMesh - %s"), Result.Num(), *NavigationDataName.ToString());
	return Result;
}

TArray<uint32> URecastNavMeshDataChunk::DetachTiles(dtNavMesh* NavMesh)
{
	TArray<uint32> Result;
	Result.Reserve(Tiles.Num());

#if WITH_RECAST	
	for (FRecastTileData& TileData : Tiles)
	{
		if (TileData.TileRef != 0)
		{
			NavMesh->removeTile(TileData.TileRef, &TileData.TileRawData->RawData, &TileData.TileDataSize);
			Result.Add(NavMesh->decodePolyIdTile(TileData.TileRef));
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
void URecastNavMeshDataChunk::GatherTiles(const dtNavMesh* NavMesh, const TArray<int32>& TileIndices)
{
	Tiles.Empty(TileIndices.Num());
	
	for (int32 TileIdx : TileIndices)
	{
		const dtMeshTile* Tile = NavMesh->getTile(TileIdx);
		if (Tile && Tile->header)
		{
			uint8* RawTileData = (uint8*)dtAlloc(Tile->dataSize, DT_ALLOC_PERM);
			FMemory::Memcpy(RawTileData, Tile->data, Tile->dataSize);
			FRecastTileData RecastTileData(NavMesh->getTileRef(Tile), Tile->dataSize, RawTileData);
			Tiles.Add(RecastTileData);
		}
	}
}
#endif// WITH_EDITOR