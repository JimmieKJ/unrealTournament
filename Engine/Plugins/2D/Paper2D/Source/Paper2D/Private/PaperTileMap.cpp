// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodySetup2D.h"

#define LOCTEXT_NAMESPACE "Paper2D"

//////////////////////////////////////////////////////////////////////////
// UPaperTileMap

UPaperTileMap::UPaperTileMap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MapWidth = 4;
	MapHeight = 4;
	TileWidth = 32;
	TileHeight = 32;
	PixelsPerUnit = 1.0f;
	SeparationPerTileX = 0.0f;
	SeparationPerTileY = 0.0f;
	SeparationPerLayer = 64.0f;
	SpriteCollisionDomain = ESpriteCollisionMode::None;

#if WITH_EDITORONLY_DATA
	SelectedLayerIndex = INDEX_NONE;
#endif

	LayerNameIndex = 0;

	static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterial(TEXT("/Paper2D/DefaultSpriteMaterial"));
	Material = DefaultMaterial.Object;
}

#if WITH_EDITOR

#include "PaperTileMapComponent.h"
#include "ComponentReregisterContext.h"

void UPaperTileMap::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	//@TODO: Determine when these are really needed, as they're seriously expensive!
	TComponentReregisterContext<UPaperTileMapComponent> ReregisterStaticComponents;

	ValidateSelectedLayerIndex();

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UPaperTileMap, MapWidth)) || (PropertyName == GET_MEMBER_NAME_CHECKED(UPaperTileMap, MapHeight)))
	{
		MapWidth = FMath::Max(MapWidth, 1);
		MapHeight = FMath::Max(MapHeight, 1);

		// Resize all of the existing layers
		for (int32 LayerIndex = 0; LayerIndex < TileLayers.Num(); ++LayerIndex)
		{
			UPaperTileLayer* TileLayer = TileLayers[LayerIndex];
			TileLayer->ResizeMap(MapWidth, MapHeight);
		}
	}

	if (!IsTemplate())
	{
		UpdateBodySetup();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPaperTileMap::PostLoad()
{
	Super::PostLoad();
	ValidateSelectedLayerIndex();
}

void UPaperTileMap::ValidateSelectedLayerIndex()
{
	if (!TileLayers.IsValidIndex(SelectedLayerIndex))
	{
		// Select the top-most visible layer
		SelectedLayerIndex = INDEX_NONE;
		for (int32 LayerIndex = 0; (LayerIndex < TileLayers.Num()) && (SelectedLayerIndex == INDEX_NONE); ++LayerIndex)
		{
			if (!TileLayers[LayerIndex]->bHiddenInEditor)
			{
				SelectedLayerIndex = LayerIndex;
			}
		}

		if ((SelectedLayerIndex == INDEX_NONE) && (TileLayers.Num() > 0))
		{
			SelectedLayerIndex = 0;
		}
	}
}

#endif


void UPaperTileMap::UpdateBodySetup()
{
	// Ensure we have the data structure for the desired collision method
	switch (SpriteCollisionDomain)
	{
	case ESpriteCollisionMode::Use3DPhysics:
		BodySetup = nullptr;
		if (BodySetup == nullptr)
		{
			BodySetup = NewObject<UBodySetup>(this);
		}
		break;
	case ESpriteCollisionMode::Use2DPhysics:
		BodySetup = nullptr;
		if (BodySetup == nullptr)
		{
			BodySetup = NewObject<UBodySetup2D>(this);
		}
		break;
	case ESpriteCollisionMode::None:
		BodySetup = nullptr;
		break;
	}

	if (SpriteCollisionDomain != ESpriteCollisionMode::None)
	{
		if (SpriteCollisionDomain == ESpriteCollisionMode::Use3DPhysics)
		{
			BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;

			BodySetup->AggGeom.BoxElems.Empty();
			for (int32 LayerIndex = 0; LayerIndex < TileLayers.Num(); ++LayerIndex)
			{
				TileLayers[LayerIndex]->AugmentBodySetup(BodySetup);
			}
		}

		//@TODO: BOX2D: Add support for 2D physics on tile maps
	}
}

FVector UPaperTileMap::GetTilePositionInLocalSpace(int32 TileX, int32 TileY, int32 LayerIndex) const
{
	//@TODO: Tile pivot issue
	const FVector PartialX = PaperAxisX * (TileX - 0.5f) * TileWidth;
	const FVector PartialY = PaperAxisY * -(TileY - 0.5f) * TileHeight;
	const FVector PartialZ = PaperAxisZ * (LayerIndex * SeparationPerLayer);

	const FVector LocalPos(PartialX + PartialY + PartialZ);
	
	return LocalPos;
}

FBoxSphereBounds UPaperTileMap::GetRenderBounds() const
{
	//@TODO: Tile pivot issue
	const float Depth = SeparationPerLayer * (TileLayers.Num() - 1);
	const float HalfThickness = 2.0f;
	const FVector TopLeft((-0.5f)*TileWidth, -HalfThickness - Depth, -(MapHeight - 0.5f) * TileHeight);
	const FVector Dimenisons(MapWidth*TileWidth, Depth + 2 * HalfThickness, MapHeight * TileHeight);

	const FBox Box(TopLeft, TopLeft + Dimenisons);
	return FBoxSphereBounds(Box);
}

UPaperTileLayer* UPaperTileMap::AddNewLayer(bool bCollisionLayer, int32 InsertionIndex)
{
	// Create the new layer
	UPaperTileLayer* NewLayer = NewObject<UPaperTileLayer>(this);
	NewLayer->SetFlags(RF_Transactional);

	NewLayer->LayerWidth = MapWidth;
	NewLayer->LayerHeight = MapHeight;
	NewLayer->DestructiveAllocateMap(NewLayer->LayerWidth, NewLayer->LayerHeight);
	NewLayer->LayerName = GenerateNewLayerName(this);
	NewLayer->bCollisionLayer = bCollisionLayer;

	// Insert the new layer
	if (TileLayers.IsValidIndex(InsertionIndex))
	{
		TileLayers.Insert(NewLayer, InsertionIndex);
	}
	else
	{
		TileLayers.Add(NewLayer);
	}

	return NewLayer;
}

FText UPaperTileMap::GenerateNewLayerName(UPaperTileMap* TileMap)
{
	// Create a set of existing names
	TSet<FString> ExistingNames;
	for (UPaperTileLayer* ExistingLayer : TileMap->TileLayers)
	{
		ExistingNames.Add(ExistingLayer->LayerName.ToString());
	}

	// Find a good name
	FText TestLayerName;
	do
	{
		TileMap->LayerNameIndex++;

		FNumberFormattingOptions NoGroupingFormat;
		NoGroupingFormat.SetUseGrouping(false);

		TestLayerName = FText::Format(LOCTEXT("NewLayerNameFormatString", "Layer {0}"), FText::AsNumber(TileMap->LayerNameIndex, &NoGroupingFormat));
	} while (ExistingNames.Contains(TestLayerName.ToString()));

	return TestLayerName;
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE