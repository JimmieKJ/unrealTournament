// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"

//////////////////////////////////////////////////////////////////////////
// UPaperTileSet

UPaperTileSet::UPaperTileSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TileWidth = 32;
	TileHeight = 32;
}

int32 UPaperTileSet::GetTileCount() const
{
	if (TileSheet != nullptr)
	{
		checkSlow((TileWidth > 0) && (TileHeight > 0));
		const int32 TextureWidth = TileSheet->GetSizeX();
		const int32 TextureHeight = TileSheet->GetSizeY();

		const int32 CellsX = (TextureWidth - (Margin * 2) + Spacing) / (TileWidth + Spacing);
		const int32 CellsY = (TextureHeight - (Margin * 2) + Spacing) / (TileHeight + Spacing);

		return CellsX * CellsY;
	}
	else
	{
		return 0;
	}
}

int32 UPaperTileSet::GetTileCountX() const
{
	if (TileSheet != nullptr)
	{
		checkSlow(TileWidth > 0);
		const int32 TextureWidth = TileSheet->GetSizeX();
		const int32 CellsX = (TextureWidth - (Margin * 2) + Spacing) / (TileWidth + Spacing);
		return CellsX;
	}
	else
	{
		return 0;
	}
}

int32 UPaperTileSet::GetTileCountY() const
{
	if (TileSheet != nullptr)
	{
		checkSlow(TileHeight > 0);
		const int32 TextureHeight = TileSheet->GetSizeY();
		const int32 CellsY = (TextureHeight - (Margin * 2) + Spacing) / (TileHeight + Spacing);
		return CellsY;
	}
	else
	{
		return 0;
	}
}

bool UPaperTileSet::GetTileUV(int32 TileIndex, /*out*/ FVector2D& Out_TileUV) const
{
	//@TODO: Performance: should cache this stuff
	if (TileSheet != nullptr)
	{
		checkSlow((TileWidth > 0) && (TileHeight > 0));
		const int32 TextureWidth = TileSheet->GetSizeX() - (Margin * 2) + Spacing;
		const int32 TextureHeight = TileSheet->GetSizeY() - (Margin * 2) + Spacing;

		const int32 CellsX = TextureWidth / (TileWidth + Spacing);
		const int32 CellsY = TextureHeight / (TileHeight + Spacing);

		const int32 NumCells = CellsX * CellsY;

		if ((TileIndex < 0) || (TileIndex >= NumCells))
		{
			return false;
		}
		else
		{
			const int32 X = TileIndex % CellsX;
			const int32 Y = TileIndex / CellsX;

			Out_TileUV.X = X * (TileWidth + Spacing) + Margin;
			Out_TileUV.Y = Y * (TileHeight + Spacing) + Margin;
			return true;
		}
	}
	else
	{
		return false;
	}
}

#if WITH_EDITOR

#include "PaperTileMapComponent.h"
#include "ComponentReregisterContext.h"

void UPaperTileSet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	Margin = FMath::Max<int32>(Margin, 0);
	Spacing = FMath::Max<int32>(Spacing, 0);

	TileWidth = FMath::Max<int32>(TileWidth, 1);
	TileHeight = FMath::Max<int32>(TileHeight, 1);

	//@TODO: Determine when these are really needed, as they're seriously expensive!
	TComponentReregisterContext<UPaperTileMapComponent> ReregisterStaticComponents;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
