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
	SeparationPerLayer = 4.0f;
	SpriteCollisionDomain = ESpriteCollisionMode::None;

#if WITH_EDITORONLY_DATA
	SelectedLayerIndex = INDEX_NONE;
	BackgroundColor = FColor(55, 55, 55);
#endif

	LayerNameIndex = 0;

	static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterial(TEXT("/Paper2D/DefaultSpriteMaterial"));
	Material = DefaultMaterial.Object;
}

#if WITH_EDITOR

#include "PaperTileMapComponent.h"
#include "ComponentReregisterContext.h"

/** Removes all components that use the specified sprite asset from their scenes for the lifetime of the class. */
class FTileMapReregisterContext
{
public:
	/** Initialization constructor. */
	FTileMapReregisterContext(UPaperTileMap* TargetAsset)
	{
		// Look at tile map components
		for (TObjectIterator<UPaperTileMapComponent> TileMapIt; TileMapIt; ++TileMapIt)
		{
			if (UPaperTileMapComponent* TestComponent = *TileMapIt)
			{
				if (TestComponent->TileMap == TargetAsset)
				{
					AddComponentToRefresh(TestComponent);
				}
			}
		}
	}

protected:
	void AddComponentToRefresh(UActorComponent* Component)
	{
		if (ComponentContexts.Num() == 0)
		{
			// wait until resources are released
			FlushRenderingCommands();
		}

		new (ComponentContexts) FComponentReregisterContext(Component);
	}

private:
	/** The recreate contexts for the individual components. */
	TIndirectArray<FComponentReregisterContext> ComponentContexts;
};

void UPaperTileMap::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	//@TODO: Determine when these are really needed, as they're seriously expensive!
	FTileMapReregisterContext ReregisterTileMapComponents(this);

	ValidateSelectedLayerIndex();

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UPaperTileMap, MapWidth)) || (PropertyName == GET_MEMBER_NAME_CHECKED(UPaperTileMap, MapHeight)))
	{
		ResizeMap(MapWidth, MapHeight, /*bForceResize=*/ true);
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

#if WITH_EDITORONLY_DATA
void UPaperTileMap::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->SourceFilePath, FAssetRegistryTag::TT_Hidden) );
	}

	Super::GetAssetRegistryTags(OutTags);
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

void UPaperTileMap::GetTileToLocalParameters(FVector& OutCornerPosition, FVector& OutStepX, FVector& OutStepY, FVector& OutOffsetYFactor) const
{
	switch (ProjectionMode)
	{
	case ETileMapProjectionMode::Orthogonal:
	default:
		OutCornerPosition = -(TileWidth * PaperAxisX * 0.5f) + (TileHeight * PaperAxisY * 0.5f);
		OutOffsetYFactor = FVector::ZeroVector;
		OutStepX = PaperAxisX * TileWidth;
		OutStepY = -PaperAxisY * TileHeight;
		break;
	case ETileMapProjectionMode::IsometricDiamond:
		OutCornerPosition = (TileHeight * PaperAxisY * 0.5f);
		OutOffsetYFactor = FVector::ZeroVector;
		OutStepX = (TileWidth * PaperAxisX * 0.5f) - (TileHeight * PaperAxisY * 0.5f);
		OutStepY = (TileWidth * PaperAxisX * -0.5f) - (TileHeight * PaperAxisY * 0.5f);
		break;
	case ETileMapProjectionMode::HexagonalStaggered:
	case ETileMapProjectionMode::IsometricStaggered:
		OutCornerPosition = -(TileWidth * PaperAxisX * 0.5f) + (TileHeight * PaperAxisY * 1.0f);
		OutOffsetYFactor = 0.5f * TileWidth * PaperAxisX;
		OutStepX = PaperAxisX * TileWidth;
		OutStepY = 0.5f * -PaperAxisY * TileHeight;
		break;
	}
}

void UPaperTileMap::GetLocalToTileParameters(FVector& OutCornerPosition, FVector& OutStepX, FVector& OutStepY, FVector& OutOffsetYFactor) const
{
	switch (ProjectionMode)
	{
	case ETileMapProjectionMode::Orthogonal:
	default:
		OutCornerPosition = -(TileWidth * PaperAxisX * 0.5f) + (TileHeight * PaperAxisY * 0.5f);
		OutOffsetYFactor = FVector::ZeroVector;
		OutStepX = PaperAxisX / TileWidth;
		OutStepY = -PaperAxisY / TileHeight;
		break;
	case ETileMapProjectionMode::IsometricDiamond:
		OutCornerPosition = (TileHeight * PaperAxisY * 0.5f);
		OutOffsetYFactor = FVector::ZeroVector;
		OutStepX = (PaperAxisX / TileWidth) - (PaperAxisY / TileHeight);
		OutStepY = (-PaperAxisX / TileWidth) - (PaperAxisY / TileHeight);
		break;
	case ETileMapProjectionMode::HexagonalStaggered:
	case ETileMapProjectionMode::IsometricStaggered:
		OutCornerPosition = -(TileWidth * PaperAxisX * 0.5f) + (TileHeight * PaperAxisY * 1.0f);
		OutOffsetYFactor = 0.5f * TileWidth * PaperAxisX;
		OutStepX = PaperAxisX / TileWidth;
		OutStepY = -PaperAxisY / TileHeight;
		break;
	}
}

void UPaperTileMap::GetTileCoordinatesFromLocalSpacePosition(const FVector& Position, int32& OutTileX, int32& OutTileY) const
{
	FVector CornerPosition;
	FVector OffsetYFactor;
	FVector ParameterAxisX;
	FVector ParameterAxisY;

	// position is in pixels
	// axis is tiles to pixels

	GetLocalToTileParameters(/*out*/ CornerPosition, /*out*/ ParameterAxisX, /*out*/ ParameterAxisY, /*out*/ OffsetYFactor);

	const FVector RelativePosition = Position - CornerPosition;
	const float ProjectionSpaceXInTiles = FVector::DotProduct(RelativePosition, ParameterAxisX);
	const float ProjectionSpaceYInTiles = FVector::DotProduct(RelativePosition, ParameterAxisY);

	OutTileX = FMath::FloorToInt(ProjectionSpaceXInTiles);
	OutTileY = FMath::FloorToInt(ProjectionSpaceYInTiles);
}

FVector UPaperTileMap::GetTilePositionInLocalSpace(float TileX, float TileY, int32 LayerIndex) const
{
	FVector CornerPosition;
	FVector OffsetYFactor;
	FVector StepX;
	FVector StepY;

	GetTileToLocalParameters(/*out*/ CornerPosition, /*out*/ StepX, /*out*/ StepY, /*out*/ OffsetYFactor);

	FVector TotalOffset;
	switch (ProjectionMode)
	{
	case ETileMapProjectionMode::Orthogonal:
	default:
		TotalOffset = CornerPosition;
		break;
	case ETileMapProjectionMode::IsometricDiamond:
		TotalOffset = CornerPosition;
		break;
	case ETileMapProjectionMode::HexagonalStaggered:
	case ETileMapProjectionMode::IsometricStaggered:
		TotalOffset = CornerPosition + ((int32)TileY & 1) * OffsetYFactor;
		break;
	}

	const FVector PartialX = TileX * StepX;
	const FVector PartialY = TileY * StepY;

	const float TotalSeparation = (SeparationPerLayer * LayerIndex) + (SeparationPerTileX * TileX) + (SeparationPerTileY * TileY);
	const FVector PartialZ = TotalSeparation * PaperAxisZ;

	const FVector LocalPos(PartialX + PartialY + PartialZ + TotalOffset);

	return LocalPos;
}

FVector UPaperTileMap::GetTileCenterInLocalSpace(float TileX, float TileY, int32 LayerIndex) const
{
	return GetTilePositionInLocalSpace(TileX + 0.5f, TileY + 0.5f, LayerIndex);
}

FBoxSphereBounds UPaperTileMap::GetRenderBounds() const
{
	const float Depth = SeparationPerLayer * (TileLayers.Num() - 1);
	const float HalfThickness = 2.0f;

	switch (ProjectionMode)
	{
		case ETileMapProjectionMode::Orthogonal:
		default:
		{
			const FVector BottomLeft((-0.5f) * TileWidth, -HalfThickness - Depth, -(MapHeight - 0.5f) * TileHeight);
			const FVector Dimensions(MapWidth*TileWidth, Depth + 2 * HalfThickness, MapHeight * TileHeight);

			const FBox Box(BottomLeft, BottomLeft + Dimensions);
			return FBoxSphereBounds(Box);
		}
		case ETileMapProjectionMode::IsometricDiamond:
		{
			 const FVector BottomLeft((-0.5f) * TileWidth * MapWidth, -HalfThickness - Depth, -MapHeight * TileHeight);
			 const FVector Dimensions(MapWidth*TileWidth, Depth + 2 * HalfThickness, (MapHeight + 1) * TileHeight);

			 const FBox Box(BottomLeft, BottomLeft + Dimensions);
			 return FBoxSphereBounds(Box);
		}
//@TODO: verify bounds for IsometricStaggered and HexagonalStaggered
	}
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

void UPaperTileMap::ResizeMap(int32 NewWidth, int32 NewHeight, bool bForceResize)
{
	if (bForceResize || (NewWidth != MapWidth) || (NewHeight != MapHeight))
	{
		MapWidth = FMath::Max(NewWidth, 1);
		MapHeight = FMath::Max(NewHeight, 1);

		// Resize all of the existing layers
		for (int32 LayerIndex = 0; LayerIndex < TileLayers.Num(); ++LayerIndex)
		{
			UPaperTileLayer* TileLayer = TileLayers[LayerIndex];
			TileLayer->ResizeMap(MapWidth, MapHeight);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE