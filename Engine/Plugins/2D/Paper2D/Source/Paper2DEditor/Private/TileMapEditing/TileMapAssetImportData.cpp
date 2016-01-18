// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "AssetData.h"
#include "TileMapAssetImportData.h"
#include "PaperTileMap.h"

//////////////////////////////////////////////////////////////////////////
// UTileMapAssetImportData

UTileMapAssetImportData::UTileMapAssetImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UTileMapAssetImportData* UTileMapAssetImportData::GetImportDataForTileMap(UPaperTileMap* TileMap/*, UFbxSkeletalMeshImportData* TemplateForCreation*/)
{
	check(TileMap);

	UTileMapAssetImportData* ImportData = Cast<UTileMapAssetImportData>(TileMap->AssetImportData);
	if (ImportData == nullptr)
	{
		ImportData = NewObject<UTileMapAssetImportData>(TileMap, NAME_None, RF_NoFlags/*, TemplateForCreation*/);

		// Try to preserve the source file path if possible
		if (TileMap->AssetImportData != nullptr)
		{
			ImportData->SourceData = TileMap->AssetImportData->SourceData;
		}

		TileMap->AssetImportData = ImportData;
	}

	return ImportData;
}
