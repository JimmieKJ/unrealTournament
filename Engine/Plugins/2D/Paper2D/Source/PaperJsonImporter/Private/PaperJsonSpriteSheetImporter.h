// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperJsonImporterPrivatePCH.h"
#include "Paper2DClasses.h"

class UPaperSpriteSheet;

//////////////////////////////////////////////////////////////////////////
// FSpriteFrame

// Represents one parsed frame in a sprite sheet
struct FSpriteFrame
{
	FName FrameName;

	FVector2D SpritePosInSheet;
	FVector2D SpriteSizeInSheet;

	FVector2D SpriteSourcePos;
	FVector2D SpriteSourceSize;

	FVector2D ImageSourceSize;

	FVector2D Pivot;

	bool bTrimmed;
	bool bRotated;
};

//////////////////////////////////////////////////////////////////////////
// FJsonPaperSpriteSheetImporter

// Parses a json from FileContents and imports / reimports a spritesheet
class FPaperJsonSpriteSheetImporter
{
public:
	FPaperJsonSpriteSheetImporter();
	bool ImportFromString(const FString& FileContents, const FString& NameForErrors);
	bool ImportFromArchive(FArchive* Archive, const FString& NameForErrors);

	bool ImportTextures(const FString& LongPackagePath, const FString& SourcePath);
	bool PerformImport(const FString& LongPackagePath, EObjectFlags Flags, UPaperSpriteSheet* SpriteSheet);

	void SetReimportData(const FString& ExistingImageName, UTexture2D* ExistingTexture, const TArray<FString>& ExistingSpriteNames, const TArray< TAssetPtr<class UPaperSprite> >& ExistingSpriteAssetPtrs);

protected:
	bool Import(TSharedPtr<FJsonObject> SpriteDescriptorObject, const FString& NameForErrors);
	UPaperSprite* FindExistingSprite(const FString& Name);

	TArray<FSpriteFrame> Frames;
	FString Image;
	UTexture2D* ImageTexture;

	bool bIsReimporting;
 	FString ExistingTextureName;					// Name of the last imported texture
	UTexture2D* ExistingTexture;					// And the texture itself
	TMap<FString, UPaperSprite*> ExistingSprites;	// Map of a sprite name (as seen in the importer) -> UPaperSprite
};

