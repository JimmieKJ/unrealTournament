// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperTileSet.h"

#include "PaperTileLayer.generated.h"

USTRUCT(BlueprintType)
struct FPaperTileInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UPaperTileSet* TileSet;

	UPROPERTY()
	int32 PackedTileIndex;

	FPaperTileInfo()
		: TileSet(nullptr)
		, PackedTileIndex(INDEX_NONE)
	{
	}

	inline bool operator==(const FPaperTileInfo& Other) const
	{
		return (Other.TileSet == TileSet) && (Other.PackedTileIndex == PackedTileIndex);
	}

	inline bool operator!=(const FPaperTileInfo& Other) const
	{
		return !(*this == Other);
	}

	bool IsValid() const
	{
		return (TileSet != nullptr) && (PackedTileIndex != INDEX_NONE);
	}
};

UCLASS()
class PAPER2D_API UPaperTileLayer : public UObject
{
	GENERATED_UCLASS_BODY()

	// Name of the layer
	UPROPERTY()
	FText LayerName;

	// Width of the layer (in tiles)
	UPROPERTY()
	int32 LayerWidth;

	// Height of the layer (in tiles)
	UPROPERTY()
	int32 LayerHeight;

#if WITH_EDITORONLY_DATA
	// Opacity of the layer (editor only)
	UPROPERTY()
	float LayerOpacity;

	// Is this layer currently hidden in the editor?
	UPROPERTY()
	bool bHiddenInEditor;
#endif

	// Should this layer be hidden in the game?
	UPROPERTY()
	bool bHiddenInGame;

	// Is this layer a collision layer?
	UPROPERTY()
	bool bCollisionLayer;

protected:
	// The allocated width of the tile data (used to handle resizing without data loss)
	UPROPERTY()
	int32 AllocatedWidth;

	// The allocated height of the tile data (used to handle resizing without data loss)
	UPROPERTY()
	int32 AllocatedHeight;

	// The allocated tile data
	UPROPERTY()
	TArray<FPaperTileInfo> AllocatedCells;

private:
	UPROPERTY()
	UPaperTileSet* TileSet_DEPRECATED;

	UPROPERTY()
	TArray<int32> AllocatedGrid_DEPRECATED;

public:
	void ConvertToTileSetPerCell();

	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End of UObject interface

	class UPaperTileMap* GetTileMap() const;

	FPaperTileInfo GetCell(int32 X, int32 Y) const;
	void SetCell(int32 X, int32 Y, const FPaperTileInfo& NewValue);

	// This is a destructive operation!
	void DestructiveAllocateMap(int32 NewWidth, int32 NewHeight);
	
	// This tries to preserve contents
	void ResizeMap(int32 NewWidth, int32 NewHeight);

	// Adds collision to the specified body setup
	void AugmentBodySetup(UBodySetup* ShapeBodySetup);
protected:
	void ReallocateAndCopyMap();
};
