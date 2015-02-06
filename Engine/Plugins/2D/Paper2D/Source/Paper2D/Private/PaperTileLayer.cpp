// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperSprite.h"
#include "ComponentReregisterContext.h"

//////////////////////////////////////////////////////////////////////////
// UPaperTileLayer

UPaperTileLayer::UPaperTileLayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LayerWidth = 4;
	LayerHeight = 4;

#if WITH_EDITORONLY_DATA
	LayerOpacity = 1.0f;

	bHiddenInEditor = false;
#endif

	DestructiveAllocateMap(LayerWidth, LayerHeight);
}

void UPaperTileLayer::DestructiveAllocateMap(int32 NewWidth, int32 NewHeight)
{
	check((NewWidth > 0) && (NewHeight > 0));

	const int32 NumCells = NewWidth * NewHeight;
	AllocatedCells.Empty(NumCells);
	AllocatedCells.AddZeroed(NumCells);

	AllocatedWidth = NewWidth;
	AllocatedHeight = NewHeight;
}

void UPaperTileLayer::ResizeMap(int32 NewWidth, int32 NewHeight)
{
	if ((LayerWidth != NewWidth) || (LayerHeight != NewHeight))
	{
		LayerWidth = NewWidth;
		LayerHeight = NewHeight;
		ReallocateAndCopyMap();
	}
}

void UPaperTileLayer::ReallocateAndCopyMap()
{
	const int32 SavedWidth = AllocatedWidth;
	const int32 SavedHeight = AllocatedHeight;
	TArray<FPaperTileInfo> SavedDesignedMap(AllocatedCells);

	DestructiveAllocateMap(LayerWidth, LayerHeight);

	const int32 CopyWidth = FMath::Min<int32>(LayerWidth, SavedWidth);
	const int32 CopyHeight = FMath::Min<int32>(LayerHeight, SavedHeight);

	for (int32 Y = 0; Y < CopyHeight; ++Y)
	{
		for (int32 X = 0; X < CopyWidth; ++X)
		{
			const int32 SrcIndex = Y*SavedWidth + X;
			const int32 DstIndex = Y*LayerWidth + X;

			AllocatedCells[DstIndex] = SavedDesignedMap[SrcIndex];
		}
	}

//	BakeMap();
}

#if WITH_EDITOR

/** Removes all components that use the specified tile map layer from their scenes for the lifetime of the class. */
class FTileMapLayerReregisterContext
{
public:
	/** Initialization constructor. */
	FTileMapLayerReregisterContext(UPaperTileLayer* TargetAsset)
	{
		// Look at tile map components
		for (TObjectIterator<UPaperTileMapComponent> MapIt; MapIt; ++MapIt)
		{
			if (UPaperTileMap* TestMap = (*MapIt)->TileMap)
			{
				if (TestMap->TileLayers.Contains(TargetAsset))
				{
					AddComponentToRefresh(*MapIt);
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



void UPaperTileLayer::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FTileMapLayerReregisterContext ReregisterExistingComponents(this);

	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UPaperTileLayer, LayerWidth)) || (PropertyName == GET_MEMBER_NAME_CHECKED(UPaperTileLayer, LayerHeight)))
	{
		// Minimum size
		LayerWidth = FMath::Max<int32>(1, LayerWidth);
		LayerHeight = FMath::Max<int32>(1, LayerHeight);

		// Resize the map, trying to preserve existing data
		ReallocateAndCopyMap();
	}
// 	else if (PropertyName == TEXT("AllocatedCells"))
// 	{
// 		BakeMap();
// 	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

UPaperTileMap* UPaperTileLayer::GetTileMap() const
{
	return CastChecked<UPaperTileMap>(GetOuter());
}

FPaperTileInfo UPaperTileLayer::GetCell(int32 X, int32 Y) const
{
	if ((X < 0) || (X >= LayerWidth) || (Y < 0) || (Y >= LayerHeight))
	{
		return FPaperTileInfo();
	}
	else
	{
		return AllocatedCells[X + (Y*LayerWidth)];
	}
}

void UPaperTileLayer::SetCell(int32 X, int32 Y, const FPaperTileInfo& NewValue)
{
	if ((X < 0) || (X >= LayerWidth) || (Y < 0) || (Y >= LayerHeight))
	{
	}
	else
	{
		AllocatedCells[X + (Y*LayerWidth)] = NewValue;
	}
}

void UPaperTileLayer::AugmentBodySetup(UBodySetup* ShapeBodySetup)
{
	if (bCollisionLayer)
	{
		//@TODO: Tile pivot issue
		//@TODO: Layer thickness issue
		const float TileWidth = GetTileMap()->TileWidth;
		const float TileHeight = GetTileMap()->TileHeight;
		const float TileThickness = 64.0f;

		//@TODO: When the origin of the component changes, this logic will need to be adjusted as well
		// The origin is currently the top left
		const float XOrigin = 0;
		const float YOrigin = 0;

		// Create a box element for every non-zero value in the layer
		for (int32 XValue = 0; XValue < LayerWidth; ++XValue)
		{
			for (int32 YValue = 0; YValue < LayerHeight; ++YValue)
			{
				if (GetCell(XValue, YValue).PackedTileIndex != 0)
				{
					FKBoxElem* NewBox = new(ShapeBodySetup->AggGeom.BoxElems) FKBoxElem(TileWidth, TileThickness, TileHeight);

					FVector BoxPosition;
					BoxPosition.X = XOrigin + XValue * TileWidth;
					BoxPosition.Y = TileThickness * 0.5f;
					BoxPosition.Z = YOrigin - YValue * TileHeight;

					NewBox->Center = BoxPosition;
				}
			}
		}
	}
}

void UPaperTileLayer::ConvertToTileSetPerCell()
{
	AllocatedCells.Empty(AllocatedGrid_DEPRECATED.Num());

	const int32 NumCells = AllocatedWidth * AllocatedHeight;
	for (int32 Index = 0; Index < NumCells; ++Index)
	{
		FPaperTileInfo* Info = new (AllocatedCells) FPaperTileInfo();
		Info->TileSet = TileSet_DEPRECATED;
		Info->PackedTileIndex = AllocatedGrid_DEPRECATED[Index];
	}
}