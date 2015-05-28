// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"

#include "TileMapAssetTypeActions.h"
#include "TileMapEditor.h"
#include "PaperTileMap.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

//////////////////////////////////////////////////////////////////////////
// FTileMapAssetTypeActions

FTileMapAssetTypeActions::FTileMapAssetTypeActions(EAssetTypeCategories::Type InAssetCategory)
	: MyAssetCategory(InAssetCategory)
{
}

FText FTileMapAssetTypeActions::GetName() const
{
	return LOCTEXT("FTileMapAssetTypeActionsName", "Tile Map");
}

FColor FTileMapAssetTypeActions::GetTypeColor() const
{
	return FColorList::BrightGold;
}

UClass* FTileMapAssetTypeActions::GetSupportedClass() const
{
	return UPaperTileMap::StaticClass();
}

void FTileMapAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UPaperTileMap* TileMap = Cast<UPaperTileMap>(*ObjIt))
		{
			TSharedRef<FTileMapEditor> NewTileMapEditor(new FTileMapEditor());
			NewTileMapEditor->InitTileMapEditor(Mode, EditWithinLevelEditor, TileMap);
		}
	}
}

uint32 FTileMapAssetTypeActions::GetCategories()
{
	return MyAssetCategory;
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE