// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperTileMap.h"
#include "PaperTileLayer.h"
#include "PaperTiledImporterFactory.generated.h"

namespace ETiledOrientation
{
	enum Type
	{
		Unknown,
		Orthogonal,
		Isometric,
		Staggered,
		Hexagonal
	};
}

// Imports a tile map (and associated textures & tile sets) exported from Tiled (http://www.mapeditor.org/)
UCLASS()
class UPaperTiledImporterFactory : public UFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	// UFactory interface
	virtual FText GetToolTip() const override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn) override;
	// End of UFactory interface

	// FReimportHandler interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;
	// End of FReimportHandler interface

protected:
	TSharedPtr<FJsonObject> ParseJSON(const FString& FileContents, const FString& NameForErrors, bool bSilent = false);

	void ParseGlobalInfoFromJSON(TSharedPtr<FJsonObject> Tree, struct FTileMapFromTiled& OutParsedInfo, const FString& NameForErrors, bool bSilent = false);

	static UObject* CreateNewAsset(UClass* AssetClass, const FString& TargetPath, const FString& DesiredName, EObjectFlags Flags);
	static UTexture2D* ImportTexture(const FString& SourceFilename, const FString& TargetSubPath);
};


struct FTileLayerFromTiled
{
	// Name of the layer
	FString Name;

	// Array of tiles
	TArray<int32> TileIndices;

	// Width and height in tiles
	int32 Width;
	int32 Height;

	// Saved layer opacity
	float Opacity;

	// Is the layer currently visible
	bool bVisible;

	// 'tilelayer' ??? - not sure what this is
	FString Type;

	// Offset
	int32 OffsetX;
	int32 OffsetY;


	FTileLayerFromTiled();

	void ParseFromJSON(TSharedPtr<FJsonObject> Tree, const FString& NameForErrors, bool bSilent = false);

	bool IsValid() const;
};

struct FTileSetFromTiled
{
	int32 FirstGID;
	FString Name;

	FString ImagePath;
	int32 ImageWidth;
	int32 ImageHeight;

	//Properties
	//TileProperties
	int32 TileOffsetX;
	int32 TileOffsetY;

	int32 Margin;
	int32 Spacing;

	int32 TileWidth;
	int32 TileHeight;

	FTileSetFromTiled();

	void ParseFromJSON(TSharedPtr<FJsonObject> Tree, const FString& NameForErrors, bool bSilent = false);

	bool IsValid() const;
};


struct FTileMapFromTiled
{
	int32 FileVersion;
	int32 Width;
	int32 Height;
	int32 TileWidth;
	int32 TileHeight;

	ETiledOrientation::Type Orientation;

	int32 HexSideLength;

	TArray<FTileSetFromTiled> TileSets;
	TArray<UPaperTileSet*> CreatedTileSetAssets;
	TArray<FTileLayerFromTiled> Layers;
	//map of properties

	FPaperTileInfo ConvertTileGIDToPaper2D(int32 GID) const;
	ETileMapProjectionMode::Type GetOrientationType() const;

	FTileMapFromTiled();
	bool IsValid() const;
};

