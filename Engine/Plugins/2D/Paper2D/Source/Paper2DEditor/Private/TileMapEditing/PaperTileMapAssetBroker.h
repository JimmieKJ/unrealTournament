// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentAssetBroker.h"
#include "PaperTileMapComponent.h"

//////////////////////////////////////////////////////////////////////////
// FPaperTileMapAssetBroker

class FPaperTileMapAssetBroker : public IComponentAssetBroker
{
public:
	UClass* GetSupportedAssetClass() override
	{
		return UPaperTileMap::StaticClass();
	}

	virtual bool AssignAssetToComponent(UActorComponent* InComponent, UObject* InAsset) override
	{
		if (UPaperTileMapComponent* RenderComp = Cast<UPaperTileMapComponent>(InComponent))
		{
			UPaperTileMap* TileMap = Cast<UPaperTileMap>(InAsset);

			if ((TileMap != nullptr) || (InAsset == nullptr))
			{
				RenderComp->TileMap = TileMap;
				return true;
			}
		}

		return false;
	}

	virtual UObject* GetAssetFromComponent(UActorComponent* InComponent) override
	{
		if (UPaperTileMapComponent* RenderComp = Cast<UPaperTileMapComponent>(InComponent))
		{
			if ((RenderComp->TileMap != nullptr) && (RenderComp->TileMap->IsAsset()))
			{
				return RenderComp->TileMap;
			}
		}

		return nullptr;
	}
};

