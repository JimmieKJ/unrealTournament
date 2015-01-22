// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TileMapAssetImportData.generated.h"

USTRUCT()
struct FTileSetImportMapping
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString SourceName;

	UPROPERTY()
	TWeakObjectPtr<class UPaperTileSet> ImportedTileSet;

	UPROPERTY()
	TWeakObjectPtr<class UTexture> ImportedTexture;
};

/**
 * Base class for import data and options used when importing a tile map
 */
UCLASS()
class PAPER2DEDITOR_API UTileMapAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FTileSetImportMapping> TileSetMap;

	static UTileMapAssetImportData* GetImportDataForTileMap(class UPaperTileMap* TileMap);
};
