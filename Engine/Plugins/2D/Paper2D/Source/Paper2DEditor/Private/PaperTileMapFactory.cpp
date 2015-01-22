// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"

#define LOCTEXT_NAMESPACE "Paper2D"

/////////////////////////////////////////////////////
// UPaperTileMapFactory

UPaperTileMapFactory::UPaperTileMapFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UPaperTileMap::StaticClass();
}

UObject* UPaperTileMapFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UPaperTileMap* NewTileMap = ConstructObject<UPaperTileMap>(Class, InParent, Name, Flags | RF_Transactional);

	if (InitialTileSet != nullptr)
	{
		NewTileMap->TileWidth = InitialTileSet->TileWidth;
		NewTileMap->TileHeight = InitialTileSet->TileHeight;
	}

	NewTileMap->AddNewLayer();

	return NewTileMap;
}

#undef LOCTEXT_NAMESPACE