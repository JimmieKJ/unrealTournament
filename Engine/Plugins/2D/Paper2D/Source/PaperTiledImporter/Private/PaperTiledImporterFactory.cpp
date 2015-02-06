// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PaperTiledImporterPrivatePCH.h"
#include "Paper2DClasses.h"
#include "Paper2DEditorClasses.h"
#include "Json.h"
#include "PaperJSONHelpers.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "PackageTools.h"

#define LOCTEXT_NAMESPACE "Paper2D"
#define TILED_IMPORT_ERROR(FormatString, ...) \
	if (!bSilent) { UE_LOG(LogPaperTiledImporter, Warning, FormatString, __VA_ARGS__); }
#define TILED_IMPORT_WARNING(FormatString, ...) \
	if (!bSilent) { UE_LOG(LogPaperTiledImporter, Warning, FormatString, __VA_ARGS__); }

//////////////////////////////////////////////////////////////////////////
// FRequiredIntField

struct FRequiredIntField
{
	int32& Value;
	const FString Key;
	int32 MinValue;

	FRequiredIntField(int32& InValue, const FString& InKey)
		: Value(InValue)
		, Key(InKey)
		, MinValue(1)
	{
	}

	FRequiredIntField(int32& InValue, const FString& InKey, int32 InMinValue)
		: Value(InValue)
		, Key(InKey)
		, MinValue(InMinValue)
	{
	}
};

bool ParseIntegerFields(FRequiredIntField* IntFieldArray, int32 ArrayCount, TSharedPtr<FJsonObject> Tree, const FString& NameForErrors, bool bSilent)
{
	bool bSuccessfullyParsed = true;

	for (int32 ArrayIndex = 0; ArrayIndex < ArrayCount; ++ArrayIndex)
	{
		const FRequiredIntField& Field = IntFieldArray[ArrayIndex];

		if (!Tree->TryGetNumberField(Field.Key, /*out*/ Field.Value))
		{
			TILED_IMPORT_ERROR(TEXT("Failed to parse '%s'.  Missing '%s' property"), *NameForErrors, *Field.Key);
			bSuccessfullyParsed = false;
			Field.Value = 0;
		}
		else
		{
			if (Field.Value < Field.MinValue)
			{
				TILED_IMPORT_ERROR(TEXT("Failed to parse '%s'.  Invalid value for '%s' (%d but must be at least %d)"), *NameForErrors, *Field.Key, Field.Value, Field.MinValue);
				bSuccessfullyParsed = false;
				Field.Value = Field.MinValue;
			}
		}
	}

	return bSuccessfullyParsed;
}


//////////////////////////////////////////////////////////////////////////
// UPaperTiledImporterFactory

UPaperTiledImporterFactory::UPaperTiledImporterFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	//bEditAfterNew = true;
	SupportedClass = UPaperTileMap::StaticClass();

	bEditorImport = true;
	bText = true;

	Formats.Add(TEXT("json;Tiled JSON file"));
}

FText UPaperTiledImporterFactory::GetToolTip() const
{
	return LOCTEXT("PaperTiledImporterFactoryDescription", "Tile maps exported from Tiled");
}

bool UPaperTiledImporterFactory::FactoryCanImport(const FString& Filename)
{
	FString FileContent;
	if (FFileHelper::LoadFileToString(/*out*/ FileContent, *Filename))
	{
		TSharedPtr<FJsonObject> DescriptorObject = ParseJSON(FileContent, FString(), /*bSilent=*/ true);
		if (DescriptorObject.IsValid())
		{
			FTileMapFromTiled GlobalInfo;
			ParseGlobalInfoFromJSON(DescriptorObject, /*out*/ GlobalInfo, FString(), /*bSilent=*/ true);

			return GlobalInfo.IsValid();
		}
	}

	return false;
}

UObject* UPaperTiledImporterFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	Flags |= RF_Transactional;

	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

 	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
 
 	bool bLoadedSuccessfully = true;
 
 	const FString CurrentFilename = UFactory::GetCurrentFilename();
 	FString CurrentSourcePath;
 	FString FilenameNoExtension;
 	FString UnusedExtension;
 	FPaths::Split(CurrentFilename, CurrentSourcePath, FilenameNoExtension, UnusedExtension);
 
 	const FString LongPackagePath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetPathName());
 
 	const FString NameForErrors(InName.ToString());
 	const FString FileContent(BufferEnd - Buffer, Buffer);
 	TSharedPtr<FJsonObject> DescriptorObject = ParseJSON(FileContent, NameForErrors);

 	UPaperTileMap* Result = nullptr;
 
	FTileMapFromTiled GlobalInfo;
	if (DescriptorObject.IsValid())
	{
		ParseGlobalInfoFromJSON(DescriptorObject, /*out*/ GlobalInfo, NameForErrors);
	}

	if (GlobalInfo.IsValid())
	{
		if (GlobalInfo.FileVersion != 1)
		{
			UE_LOG(LogPaperTiledImporter, Warning, TEXT("JSON exported from Tiled in file '%s' has an unknown version %d (expected version 1).  Parsing will continue but some things may not import correctly"), *NameForErrors, GlobalInfo.FileVersion);
		}

		// Load the tile sets
		const TArray<TSharedPtr<FJsonValue>>* TileSetDescriptors;
		if (DescriptorObject->TryGetArrayField(TEXT("tilesets"), /*out*/ TileSetDescriptors))
		{
			for (TSharedPtr<FJsonValue> TileSetDescriptor : *TileSetDescriptors)
			{
				FTileSetFromTiled& TileSet = *new (GlobalInfo.TileSets) FTileSetFromTiled();
				TileSet.ParseFromJSON(TileSetDescriptor->AsObject(), NameForErrors);
				bLoadedSuccessfully = bLoadedSuccessfully && TileSet.IsValid();
			}
		}
		else
		{
			UE_LOG(LogPaperTiledImporter, Warning, TEXT("JSON exported from Tiled in file '%s' has no tile sets."), *NameForErrors);
			bLoadedSuccessfully = false;
		}

		// Load the layers
		const TArray<TSharedPtr<FJsonValue>>* LayerDescriptors;
		if (DescriptorObject->TryGetArrayField(TEXT("layers"), /*out*/ LayerDescriptors))
		{
			for (TSharedPtr<FJsonValue> LayerDescriptor : *LayerDescriptors)
			{
				FTileLayerFromTiled& TileLayer = *new (GlobalInfo.Layers) FTileLayerFromTiled();
				TileLayer.ParseFromJSON(LayerDescriptor->AsObject(), NameForErrors);
				bLoadedSuccessfully = bLoadedSuccessfully && TileLayer.IsValid();
			}
		}
		else
		{
			UE_LOG(LogPaperTiledImporter, Warning, TEXT("JSON exported from Tiled in file '%s' has no layers."), *NameForErrors);
			bLoadedSuccessfully = false;
		}

		// Load the properties
		//@TODO: What to do with Tiled properties?



		// Create the new tile map asset and import basic/global data
		Result = NewNamedObject<UPaperTileMap>(InParent, InName, Flags);

		Result->Modify();
		Result->MapWidth = GlobalInfo.Width;
		Result->MapHeight = GlobalInfo.Height;
		Result->TileWidth = GlobalInfo.TileWidth;
		Result->TileHeight = GlobalInfo.TileHeight;
		Result->SeparationPerTileX = 0.0f;
		Result->SeparationPerTileY = 0.0f;
		Result->SeparationPerLayer = -1.0f;
		Result->ProjectionMode = GlobalInfo.GetOrientationType();

		// Create the tile sets
		for (const FTileSetFromTiled& TileSetData : GlobalInfo.TileSets)
		{
			if (TileSetData.IsValid())
			{
				const FString TargetTileSetPath = LongPackagePath;
				const FString TargetTexturePath = LongPackagePath + TEXT("/Textures");

				UPaperTileSet* TileSetAsset = CastChecked<UPaperTileSet>(CreateNewAsset(UPaperTileSet::StaticClass(), TargetTileSetPath, TileSetData.Name, Flags));
				TileSetAsset->Modify();

				TileSetAsset->TileWidth = TileSetData.TileWidth;
				TileSetAsset->TileHeight = TileSetData.TileHeight;
				TileSetAsset->Margin = TileSetData.Margin;
				TileSetAsset->Spacing = TileSetData.Spacing;
				TileSetAsset->DrawingOffset = FIntPoint(TileSetData.TileOffsetX, TileSetData.TileOffsetY);

				//@TODO: Ignoring ImageWidth and ImageHeight, and Properties/TileProperties aren't even loaded yet

				// Import the texture
				const FString SourceImageFilename = FPaths::Combine(*CurrentSourcePath, *TileSetData.ImagePath);
				TileSetAsset->TileSheet = ImportTexture(SourceImageFilename, TargetTexturePath);

				if (TileSetAsset->TileSheet == nullptr)
				{
					UE_LOG(LogPaperTiledImporter, Warning, TEXT("Failed to import tile set image '%s' referenced from tile set '%s'."), *TileSetData.ImagePath, *TileSetData.Name);
					bLoadedSuccessfully = false;
				}

				TileSetAsset->PostEditChange();
				GlobalInfo.CreatedTileSetAssets.Add(TileSetAsset);
			}
			else
			{
				GlobalInfo.CreatedTileSetAssets.Add(nullptr);
			}
		}

		if (GlobalInfo.CreatedTileSetAssets.Num() > 0)
		{
			// Bind our selected tile set to the first tile set that was imported so something is already picked
			Result->SelectedTileSet = GlobalInfo.CreatedTileSetAssets[0];
		}

		// Create the layers
		for (const FTileLayerFromTiled& LayerData : GlobalInfo.Layers)
		{
			if (LayerData.IsValid())
			{
				UPaperTileLayer* NewLayer = NewObject<UPaperTileLayer>(Result);
				NewLayer->SetFlags(RF_Transactional);

				NewLayer->LayerName = FText::FromString(LayerData.Name);
				NewLayer->bHiddenInEditor = !LayerData.bVisible;
				NewLayer->LayerOpacity = FMath::Clamp<float>(LayerData.Opacity, 0.0f, 1.0f);
				//@TODO: No support for Type, OffsetX, or OffsetY

				NewLayer->LayerWidth = LayerData.Width;
				NewLayer->LayerHeight = LayerData.Height;
				NewLayer->DestructiveAllocateMap(LayerData.Width, LayerData.Height);

				int32 SourceIndex = 0;
				for (int32 Y = 0; Y < LayerData.Height; ++Y)
				{
					for (int32 X = 0; X < LayerData.Width; ++X)
					{
						const int32 SourceTileGID = LayerData.TileIndices[SourceIndex++];
						const FPaperTileInfo CellContents = GlobalInfo.ConvertTileGIDToPaper2D(SourceTileGID);
						NewLayer->SetCell(X, Y, CellContents);
					}
				}

				//@TODO: Handle TileIndices!

				Result->TileLayers.Add(NewLayer);
			}
		}

		Result->PostEditChange();
	}
 	else
 	{
 		// Failed to parse the JSON
 		bLoadedSuccessfully = false;
 	}

	if (Result != nullptr)
	{
		// Store the current file path and timestamp for re-import purposes
		UTileMapAssetImportData* ImportData = UTileMapAssetImportData::GetImportDataForTileMap(Result);
		ImportData->SourceFilePath = FReimportManager::SanitizeImportFilename(CurrentFilename, Result);
		ImportData->SourceFileTimestamp = IFileManager::Get().GetTimeStamp(*CurrentFilename).ToString();

		//GlobalInfo.TileSets
	}

	FEditorDelegates::OnAssetPostImport.Broadcast(this, Result);

	return Result;
}

TSharedPtr<FJsonObject> UPaperTiledImporterFactory::ParseJSON(const FString& FileContents, const FString& NameForErrors, bool bSilent)
{
	// Load the file up (JSON format)
	if (!FileContents.IsEmpty())
	{
		const TSharedRef< TJsonReader<> >& Reader = TJsonReaderFactory<>::Create(FileContents);

		TSharedPtr<FJsonObject> DescriptorObject;
		if (FJsonSerializer::Deserialize(Reader, /*out*/ DescriptorObject) && DescriptorObject.IsValid())
		{
			// File was loaded and deserialized OK!
			return DescriptorObject;
		}
		else
		{
			if (!bSilent)
			{
				//@TODO: PAPER2D: How to correctly surface import errors to the user?
				UE_LOG(LogPaperTiledImporter, Warning, TEXT("Failed to parse tile map JSON file '%s'.  Error: '%s'"), *NameForErrors, *Reader->GetErrorMessage());
			}
			return nullptr;
		}
	}
	else
	{
		if (!bSilent)
		{
			UE_LOG(LogPaperTiledImporter, Warning, TEXT("Tile map JSON file '%s' was empty.  This tile map cannot be imported."), *NameForErrors);
		}
		return nullptr;
	}
}

bool UPaperTiledImporterFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (UPaperTileMap* TileMap = Cast<UPaperTileMap>(Obj))
	{
		if (TileMap->AssetImportData != nullptr)
		{
			OutFilenames.Add(FReimportManager::ResolveImportFilename(TileMap->AssetImportData->SourceFilePath, TileMap));
		}
		else
		{
			OutFilenames.Add(TEXT(""));
		}
		return true;
	}
	return false;
}

void UPaperTiledImporterFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (UPaperTileMap* TileMap = Cast<UPaperTileMap>(Obj))
	{
		if (ensure(NewReimportPaths.Num() == 1))
		{
			UTileMapAssetImportData* ImportData = UTileMapAssetImportData::GetImportDataForTileMap(TileMap);

			ImportData->SourceFilePath = FReimportManager::SanitizeImportFilename(NewReimportPaths[0], TileMap);
		}
	}
}

EReimportResult::Type UPaperTiledImporterFactory::Reimport(UObject* Obj)
{
	if (UPaperTileMap* TileMap = Cast<UPaperTileMap>(Obj))
	{
		//@TODO: Not implemented yet
		ensureMsg(false, TEXT("Tile map reimport is not implemented yet"));
	}
	return EReimportResult::Failed;
}

int32 UPaperTiledImporterFactory::GetPriority() const
{
	return ImportPriority;
}


UObject* UPaperTiledImporterFactory::CreateNewAsset(UClass* AssetClass, const FString& TargetPath, const FString& DesiredName, EObjectFlags Flags)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	// Create a unique package name and asset name for the frame
	const FString TentativePackagePath = PackageTools::SanitizePackageName(TargetPath + TEXT("/") + DesiredName);
	FString DefaultSuffix;
	FString AssetName;
	FString PackageName;
	AssetToolsModule.Get().CreateUniqueAssetName(TentativePackagePath, /*out*/ DefaultSuffix, /*out*/ PackageName, /*out*/ AssetName);

	// Create a package for the asset
	UObject* OuterForAsset = CreatePackage(nullptr, *PackageName);

	// Create a frame in the package
	UObject* NewAsset = ConstructObject<UObject>(AssetClass, OuterForAsset, *AssetName, Flags);
	FAssetRegistryModule::AssetCreated(NewAsset);

	return NewAsset;
}

UTexture2D* UPaperTiledImporterFactory::ImportTexture(const FString& SourceFilename, const FString& TargetSubPath)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	TArray<FString> FileNames;
	FileNames.Add(SourceFilename);

	//@TODO: Avoid the first compression, since we're going to recompress
	TArray<UObject*> ImportedAssets = AssetToolsModule.Get().ImportAssets(FileNames, TargetSubPath);
	UTexture2D* ImportedTexture = ImportedTexture = (ImportedAssets.Num() > 0) ? Cast<UTexture2D>(ImportedAssets[0]) : nullptr;

	if (ImportedTexture != nullptr)
	{
		// Change the compression settings
		ImportedTexture->Modify();
		ImportedTexture->LODGroup = TEXTUREGROUP_UI;
		ImportedTexture->CompressionSettings = TC_EditorIcon;
		ImportedTexture->Filter = TF_Nearest;
		ImportedTexture->PostEditChange();
	}

	return ImportedTexture;
}

void UPaperTiledImporterFactory::ParseGlobalInfoFromJSON(TSharedPtr<FJsonObject> Tree, FTileMapFromTiled& OutParsedInfo, const FString& NameForErrors, bool bSilent)
{
	bool bSuccessfullyParsed = true;

	// Parse all of the required integer fields
	FRequiredIntField IntFields[] = {
		FRequiredIntField( OutParsedInfo.FileVersion, TEXT("version") ),
		FRequiredIntField( OutParsedInfo.Width, TEXT("width") ),
		FRequiredIntField( OutParsedInfo.Height, TEXT("height") ),
		FRequiredIntField( OutParsedInfo.TileWidth, TEXT("tilewidth") ),
		FRequiredIntField( OutParsedInfo.TileHeight, TEXT("tileheight") )
	};
	bSuccessfullyParsed = bSuccessfullyParsed && ParseIntegerFields(IntFields, ARRAY_COUNT(IntFields), Tree, NameForErrors, bSilent);

	// Parse hexsidelength if present
	FRequiredIntField OptionalIntFields[] = {
		FRequiredIntField(OutParsedInfo.HexSideLength, TEXT("hexsidelength"), 0)
	};
	ParseIntegerFields(OptionalIntFields, ARRAY_COUNT(OptionalIntFields), Tree, NameForErrors, /*bSilent=*/ true);

	//@TODO:
	// Parse staggeraxis and staggerindex (currently not supported by the renderer anyways)

	// Parse the orientation
	const FString OrientationModeStr = FPaperJSONHelpers::ReadString(Tree, TEXT("orientation"), TEXT(""));
	if (OrientationModeStr == TEXT("orthogonal"))
	{
		OutParsedInfo.Orientation = ETiledOrientation::Orthogonal;
	}
	else if (OrientationModeStr == TEXT("isometric"))
	{
		OutParsedInfo.Orientation = ETiledOrientation::Isometric;
	}
	else if (OrientationModeStr == TEXT("staggered"))
	{
		OutParsedInfo.Orientation = ETiledOrientation::Staggered;
	}
	else if (OrientationModeStr == TEXT("hexagonal"))
	{
		OutParsedInfo.Orientation = ETiledOrientation::Hexagonal;
	}
	else
	{
		TILED_IMPORT_ERROR(TEXT("Failed to parse '%s'.  Invalid value for '%s' (%s but expected 'orthogonal', 'isometric', 'staggered', or 'hexagonal')"), *NameForErrors, TEXT("orientation"), *OrientationModeStr);
		bSuccessfullyParsed = false;
		OutParsedInfo.Orientation = ETiledOrientation::Unknown;
	}
}

//////////////////////////////////////////////////////////////////////////
// FTileMapFromTiled

FTileMapFromTiled::FTileMapFromTiled()
	: FileVersion(0)
	, Width(0)
	, Height(0)
	, TileWidth(0)
	, TileHeight(0)
	, Orientation(ETiledOrientation::Unknown)
	, HexSideLength(0)
{
}

bool FTileMapFromTiled::IsValid() const
{
	return (FileVersion != 0) && (Width > 0) && (Height > 0) && (TileWidth > 0) && (TileHeight > 0) && (Orientation != ETiledOrientation::Unknown);
}

FPaperTileInfo FTileMapFromTiled::ConvertTileGIDToPaper2D(int32 GID) const
{
	// Clear the mirroring / rotation bits, we don't support those yet
	//@TODO: Handle mirroring/flipping flags
	const int32 Flags = GID >> 29;
	const int32 TileIndex = GID & ~(7U << 29);

	FPaperTileInfo Result;

	for (int32 SetIndex = TileSets.Num() - 1; SetIndex >= 0; SetIndex--)
	{
		const int32 RelativeIndex = TileIndex - TileSets[SetIndex].FirstGID;
		if (RelativeIndex >= 0)
		{
			// We've found the source tile set and are done searching, but only import a non-null if that tile set imported successfully
			if (UPaperTileSet* Set = CreatedTileSetAssets[SetIndex])
			{
				Result.TileSet = Set;
				Result.PackedTileIndex = RelativeIndex;
			}
			break;
		}
	}

	return Result;
}

ETileMapProjectionMode::Type FTileMapFromTiled::GetOrientationType() const
{
	switch (Orientation)
	{
	case ETiledOrientation::Isometric:
		return ETileMapProjectionMode::IsometricDiamond;
	case ETiledOrientation::Staggered:
		return ETileMapProjectionMode::IsometricStaggered;
	case ETiledOrientation::Hexagonal:
		return ETileMapProjectionMode::HexagonalStaggered;
	case ETiledOrientation::Orthogonal:
	default:
		return ETileMapProjectionMode::Orthogonal;
	};
}

//////////////////////////////////////////////////////////////////////////
// FTileSetFromTiled

FTileSetFromTiled::FTileSetFromTiled()
	: FirstGID(INDEX_NONE)
	, ImageWidth(0)
	, ImageHeight(0)
	, TileOffsetX(0)
	, TileOffsetY(0)
	, Margin(0)
	, Spacing(0)
	, TileWidth(0)
	, TileHeight(0)
{
}

void FTileSetFromTiled::ParseFromJSON(TSharedPtr<FJsonObject> Tree, const FString& NameForErrors, bool bSilent)
{
	bool bSuccessfullyParsed = true;

	// Parse all of the integer fields
	FRequiredIntField IntFields[] = {
		FRequiredIntField( FirstGID, TEXT("firstgid"), 1 ),
		FRequiredIntField( ImageWidth, TEXT("imagewidth"), 1 ),
		FRequiredIntField( ImageHeight, TEXT("imageheight"), 1 ),
		FRequiredIntField( Margin, TEXT("margin"), 0 ),
		FRequiredIntField( Spacing, TEXT("spacing"), 0 ),
		FRequiredIntField( TileWidth, TEXT("tilewidth"), 1 ),
		FRequiredIntField( TileHeight, TEXT("tileheight"), 1 )
	};

	bSuccessfullyParsed = bSuccessfullyParsed && ParseIntegerFields(IntFields, ARRAY_COUNT(IntFields), Tree, NameForErrors, bSilent);

	// Parse the tile offset
	if (bSuccessfullyParsed)
	{
		FIntPoint TileOffsetTemp;
		if (FPaperJSONHelpers::ReadIntPoint(Tree, TEXT("tileoffset"), /*out*/ TileOffsetTemp))
		{
			TileOffsetX = TileOffsetTemp.X;
			TileOffsetY = TileOffsetTemp.Y;
		}
		else
		{
			TILED_IMPORT_ERROR(TEXT("Failed to parse '%s'.  Invalid or missing value for '%s'"), *NameForErrors, TEXT("tileoffset"));
			bSuccessfullyParsed = false;
		}
	}

	//@TODO: Parse the properties
	//@TODO: Parse the tile properties

	// Parse the tile set name
	Name = FPaperJSONHelpers::ReadString(Tree, TEXT("name"), TEXT(""));
	if (Name.IsEmpty())
	{
		TILED_IMPORT_WARNING(TEXT("Expected a non-empty name for each tile set in '%s', generating a new name"), *NameForErrors);
		Name = FString::Printf(TEXT("TileSetStartingAt%d"), FirstGID);
	}

	// Parse the image path
	ImagePath = FPaperJSONHelpers::ReadString(Tree, TEXT("image"), TEXT(""));
	if (ImagePath.IsEmpty())
	{
		TILED_IMPORT_ERROR(TEXT("Failed to parse '%s'.  Invalid value for '%s' (%s but expected a path to an image)"), *NameForErrors, TEXT("image"), *ImagePath);
		bSuccessfullyParsed = false;
	}
}

bool FTileSetFromTiled::IsValid() const
{
	return (TileWidth > 0) && (TileHeight > 0) && (FirstGID > 0);
}

//////////////////////////////////////////////////////////////////////////
// FTileLayerFromTiled

FTileLayerFromTiled::FTileLayerFromTiled()
	: Width(0)
	, Height(0)
	, Opacity(1.0f)
	, bVisible(true)
	, OffsetX(0)
	, OffsetY(0)
{
}

void FTileLayerFromTiled::ParseFromJSON(TSharedPtr<FJsonObject> Tree, const FString& NameForErrors, bool bSilent)
{
	bool bSuccessfullyParsed = true;

	// Parse all of the integer fields
	FRequiredIntField IntFields[] = {
		FRequiredIntField( Width, TEXT("width"), 1 ),
		FRequiredIntField( Height, TEXT("height"), 1 ),
		FRequiredIntField( OffsetX, TEXT("x"), 0 ),
		FRequiredIntField( OffsetY, TEXT("y"), 0 )
	};

	bSuccessfullyParsed = bSuccessfullyParsed && ParseIntegerFields(IntFields, ARRAY_COUNT(IntFields), Tree, NameForErrors, bSilent);

	if (!Tree->TryGetBoolField(TEXT("visible"), /*out*/ bVisible))
	{
		bVisible = true;
	}

	if (!FPaperJSONHelpers::ReadFloatNoDefault(Tree, TEXT("opacity"), /*out*/ Opacity))
	{
		Opacity = 1.0f;
	}

	if (!Tree->TryGetStringField(TEXT("name"), /*out*/ Name))
	{
		TILED_IMPORT_ERROR(TEXT("Failed to parse '%s'.  Expected a layer name"), *NameForErrors);
		bSuccessfullyParsed = false;
	}

	if (!Tree->TryGetStringField(TEXT("type"), /*out*/ Type))
	{
		//@TODO: Figure out what to do with the layer type!
		TILED_IMPORT_ERROR(TEXT("Failed to parse '%s'.  Invalid value for '%s' (%s but expected a ??? )"), *NameForErrors, TEXT("type"), *Type);
		bSuccessfullyParsed = false;
	}

	const TArray<TSharedPtr<FJsonValue>>* DataArray;
	if (Tree->TryGetArrayField(TEXT("data"), /*out*/ DataArray))
	{
		TileIndices.Reserve(DataArray->Num());
		for (TSharedPtr<FJsonValue> TileEntry : *DataArray)
		{
			int32 TileID = (int32)TileEntry->AsNumber();
			TileIndices.Add(TileID);
		}
	}
	else
	{
		TILED_IMPORT_ERROR(TEXT("Failed to parse '%s'.  Missing layer data for layer '%s'"), *NameForErrors, *Name);
		bSuccessfullyParsed = false;
	}
}

bool FTileLayerFromTiled::IsValid() const
{
	return (Width > 0) && (Height > 0) && (TileIndices.Num() == (Width*Height));
}

//////////////////////////////////////////////////////////////////////////

#undef TILED_IMPORT_ERROR
#undef LOCTEXT_NAMESPACE