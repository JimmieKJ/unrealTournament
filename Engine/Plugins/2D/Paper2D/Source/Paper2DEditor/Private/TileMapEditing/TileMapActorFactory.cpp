// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "AssetData.h"

//////////////////////////////////////////////////////////////////////////
// UTileMapActorFactory

UTileMapActorFactory::UTileMapActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = NSLOCTEXT("Paper2D", "TileMapFactoryDisplayName", "Paper2D Tile Map");
	NewActorClass = APaperTileMapActor::StaticClass();
}

void UTileMapActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	APaperTileMapActor* TypedActor = CastChecked<APaperTileMapActor>(NewActor);
	UPaperTileMapComponent* RenderComponent = TypedActor->GetRenderComponent();
	check(RenderComponent);

	if (UPaperTileMap* TileMap = Cast<UPaperTileMap>(Asset))
	{
		GEditor->SetActorLabelUnique(NewActor, TileMap->GetName());

		RenderComponent->UnregisterComponent();
		RenderComponent->TileMap = TileMap;
		RenderComponent->RegisterComponent();
	}
	else if (UPaperTileSet* TileSet = Cast<UPaperTileSet>(Asset))
	{
		GEditor->SetActorLabelUnique(NewActor, TileSet->GetName());

		if (RenderComponent->TileMap != nullptr)
		{
			RenderComponent->UnregisterComponent();
			RenderComponent->TileMap->TileWidth = TileSet->TileWidth;
			RenderComponent->TileMap->TileHeight = TileSet->TileHeight;
			RenderComponent->RegisterComponent();
		}
	}

	if (RenderComponent->OwnsTileMap())
	{
		RenderComponent->TileMap->AddNewLayer();
	}
}

void UTileMapActorFactory::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	if (APaperTileMapActor* TypedActor = Cast<APaperTileMapActor>(CDO))
	{
		UPaperTileMapComponent* RenderComponent = TypedActor->GetRenderComponent();
		check(RenderComponent);

		if (UPaperTileMap* TileMap = Cast<UPaperTileMap>(Asset))
		{
			RenderComponent->TileMap = TileMap;
		}
		else if (UPaperTileSet* TileSet = Cast<UPaperTileSet>(Asset))
		{
			if (RenderComponent->TileMap != nullptr)
			{
				RenderComponent->TileMap->TileWidth = TileSet->TileWidth;
				RenderComponent->TileMap->TileHeight = TileSet->TileHeight;
			}
		}

		if (RenderComponent->OwnsTileMap())
		{
			RenderComponent->TileMap->AddNewLayer();
		}
	}
}

bool UTileMapActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (AssetData.IsValid())
	{
		UClass* AssetClass = AssetData.GetClass();
		if ((AssetClass != nullptr) && (AssetClass->IsChildOf(UPaperTileMap::StaticClass()) || AssetClass->IsChildOf(UPaperTileSet::StaticClass())))
		{
			return true;
		}
		else
		{
			OutErrorMsg = NSLOCTEXT("Paper2D", "CanCreateActorFrom_NoTileMap", "No tile map was specified.");
			return false;
		}
	}
	else
	{
		return true;
	}
}
