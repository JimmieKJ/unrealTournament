// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "PaperSprite.h"
#include "PaperTileMap.generated.h"

// The different kinds of projection modes supported
UENUM()
namespace ETileMapProjectionMode
{
	enum Type
	{
		// Square tile layout
		Orthogonal,

		// Isometric tile layout (shaped like a diamond)
		IsometricDiamond,

		// Isometric tile layout (roughly in a square with alternating rows staggered).  Warning: Not fully supported yet.
		IsometricStaggered,

		// Hexagonal tile layout (roughly in a square with alternating rows staggered).  Warning: Not fully supported yet.
		HexagonalStaggered
	};
}

// A tile map is a 2D grid with a defined width and height (in tiles).  There can be multiple layers, each of which can specify which tile should appear in each cell of the map for that layer.
UCLASS()
class PAPER2D_API UPaperTileMap : public UObject
{
	GENERATED_UCLASS_BODY()

	// Width of map (in tiles)
	UPROPERTY(Category=Setup, EditAnywhere, BlueprintReadOnly, meta=(UIMin=1, ClampMin=1))
	int32 MapWidth;

	// Height of map (in tiles)
	UPROPERTY(Category=Setup, EditAnywhere, BlueprintReadOnly, meta=(UIMin=1, ClampMin=1))
	int32 MapHeight;

	// Width of one tile (in pixels)
	UPROPERTY(Category=Setup, EditAnywhere, BlueprintReadOnly, meta=(UIMin=1, ClampMin=1))
	int32 TileWidth;

	// Height of one tile (in pixels)
	UPROPERTY(Category=Setup, EditAnywhere, BlueprintReadOnly, meta=(UIMin=1, ClampMin=1))
	int32 TileHeight;

	// Pixels per Unreal Unit (pixels per cm) - Note: Currently unused!
	UPROPERTY(Category=Setup, EditAnywhere)
	float PixelsPerUnit;

	// The Z-separation incurred as you travel in X (not strictly applied, batched tiles will be put at the same Z level) 
	UPROPERTY(Category=Setup, EditAnywhere, AdvancedDisplay)
	float SeparationPerTileX;

	// The Z-separation incurred as you travel in Y (not strictly applied, batched tiles will be put at the same Z level) 
	UPROPERTY(Category=Setup, EditAnywhere, AdvancedDisplay)
	float SeparationPerTileY;
	
	// The Z-separation between each layer of the tile map
	UPROPERTY(Category=Setup, EditAnywhere, BlueprintReadOnly)
	float SeparationPerLayer;

	// Last tile set that was selected when editing the tile map
	UPROPERTY()
	TAssetPtr<class UPaperTileSet> SelectedTileSet;

	// The material to use on a tile map instance if not overridden
	UPROPERTY(Category=Setup, EditAnywhere, BlueprintReadOnly)
	UMaterialInterface* Material;

	// The list of layers
	UPROPERTY(Instanced, Category=Sprite, BlueprintReadOnly)
	TArray<class UPaperTileLayer*> TileLayers;

	// Collision domain (no collision, 2D, or 3D)
	UPROPERTY(Category=Collision, EditAnywhere, BlueprintReadOnly)
	TEnumAsByte<ESpriteCollisionMode::Type> SpriteCollisionDomain;

	// Tile map type
	UPROPERTY(Category=Setup, EditAnywhere, BlueprintReadOnly)
	TEnumAsByte<ETileMapProjectionMode::Type> ProjectionMode;

	// Baked physics data.
	UPROPERTY()
	class UBodySetup* BodySetup;

public:
#if WITH_EDITORONLY_DATA
	/** Importing data and options used for this tile map */
	UPROPERTY(Category=ImportSettings, VisibleAnywhere, Instanced)
	class UAssetImportData* AssetImportData;

	/** The currently selected layer index */
	UPROPERTY()
	int32 SelectedLayerIndex;

	/** The background color displayed in the tile map editor */
	UPROPERTY(Category=Setup, EditAnywhere)
	FLinearColor BackgroundColor;
#endif

	/** The naming index to start at when trying to create a new layer */
	UPROPERTY()
	int32 LayerNameIndex;

public:
	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	void ValidateSelectedLayerIndex();
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const;
#endif
	// End of UObject interface

	// Returns the tile coordinates of the specified local space position
	void GetTileCoordinatesFromLocalSpacePosition(const FVector& Position, int32& OutTileX, int32& OutTileY) const;

	// Returns the top left corner of the specified tile in local space
	FVector GetTilePositionInLocalSpace(float TileX, float TileY, int32 LayerIndex = 0) const;

	// Returns the center of the specified tile in local space
	FVector GetTileCenterInLocalSpace(float TileX, float TileY, int32 LayerIndex = 0) const;


	void GetTileToLocalParameters(FVector& OutCornerPosition, FVector& OutStepX, FVector& OutStepY, FVector& OutOffsetYFactor) const;
	void GetLocalToTileParameters(FVector& OutCornerPosition, FVector& OutStepX, FVector& OutStepY, FVector& OutOffsetYFactor) const;


	FBoxSphereBounds GetRenderBounds() const;

	// Creates and adds a new layer and returns it
	class UPaperTileLayer* AddNewLayer(bool bCollisionLayer = false, int32 InsertionIndex = INDEX_NONE);

	// Creates a reasonable new layer name
	static FText GenerateNewLayerName(UPaperTileMap* TileMap);

	// Resize the tile map and all layers
	void ResizeMap(int32 NewWidth, int32 NewHeight, bool bForceResize = true);
protected:
	virtual void UpdateBodySetup();
};
