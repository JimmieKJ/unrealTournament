// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PaperJsonImporterPrivatePCH.h"
#include "Paper2DClasses.h"
#include "Json.h"
#include "PaperJSONHelpers.h"
#include "PaperJsonSpriteSheetImporter.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "PackageTools.h"
#include "PaperSpriteSheet.h"

TSharedPtr<FJsonObject> ParseJSON(const FString& FileContents, const FString& NameForErrors)
{
	// Load the file up (JSON format)
	if (!FileContents.IsEmpty())
	{
		const TSharedRef< TJsonReader<> >& Reader = TJsonReaderFactory<>::Create(FileContents);

		TSharedPtr<FJsonObject> SpriteDescriptorObject;
		if (FJsonSerializer::Deserialize(Reader, SpriteDescriptorObject) && SpriteDescriptorObject.IsValid())
		{
			// File was loaded and deserialized OK!
			return SpriteDescriptorObject;
		}
		else
		{
			UE_LOG(LogPaperJsonImporter, Warning, TEXT("Failed to parse sprite descriptor file '%s'.  Error: '%s'"), *NameForErrors, *Reader->GetErrorMessage());
			return nullptr;
		}
	}
	else
	{
		UE_LOG(LogPaperJsonImporter, Warning, TEXT("Sprite descriptor file '%s' was empty.  This sprite cannot be imported."), *NameForErrors);
		return nullptr;
	}
}

TSharedPtr<FJsonObject> ParseJSON(FArchive* const Stream, const FString& NameForErrors)
{
	const TSharedRef< TJsonReader<> >& Reader = TJsonReaderFactory<>::Create(Stream);

	TSharedPtr<FJsonObject> SpriteDescriptorObject;
	if (FJsonSerializer::Deserialize(Reader, SpriteDescriptorObject) && SpriteDescriptorObject.IsValid())
	{
		// File was loaded and deserialized OK!
		return SpriteDescriptorObject;
	}
	else
	{
		UE_LOG(LogPaperJsonImporter, Warning, TEXT("Failed to parse sprite descriptor file '%s'.  Error: '%s'"), *NameForErrors, *Reader->GetErrorMessage());
		return nullptr;
	}
}

bool ParseMetaBlock(const FString& NameForErrors, TSharedPtr<FJsonObject>& SpriteDescriptorObject, FString& OutImage)
{
	bool bLoadedSuccessfully = true;
	TSharedPtr<FJsonObject> MetaBlock = FPaperJSONHelpers::ReadObject(SpriteDescriptorObject, TEXT("meta"));
	if (MetaBlock.IsValid())
	{
		// Example contents:
		//   "app": "Adobe Flash CS6",
		//   "version": "12.0.0.481",        (ignored)
		//   "image": "MySprite.png",
		//   "format": "RGBA8888",           (ignored)
		//   "size": {"w":2048,"h":2048},    (ignored)
		//   "scale": "1"                    (ignored)

		const FString AppName = FPaperJSONHelpers::ReadString(MetaBlock, TEXT("app"), TEXT(""));
		OutImage = FPaperJSONHelpers::ReadString(MetaBlock, TEXT("image"), TEXT(""));

		const FString FlashPrefix(TEXT("Adobe Flash"));
		const FString TexturePackerPrefix(TEXT("http://www.codeandweb.com/texturepacker"));

		if (AppName.StartsWith(FlashPrefix) || AppName.StartsWith(TexturePackerPrefix))
		{
			// Cool, we (mostly) know how to handle these sorts of files!
			UE_LOG(LogPaperJsonImporter, Log, TEXT("Parsing sprite sheet exported from '%s'"), *AppName);
		}
		else if (!AppName.IsEmpty())
		{
			// It's got an app tag inside a meta block, so we'll take a crack at it
			UE_LOG(LogPaperJsonImporter, Warning, TEXT("Unexpected 'app' named '%s' while parsing sprite descriptor file '%s'.  Parsing will continue but the format may not be fully supported"), *AppName, *NameForErrors);
		}
		else
		{
			// Probably not a sprite sheet
			UE_LOG(LogPaperJsonImporter, Warning, TEXT("Failed to parse sprite descriptor file '%s'.  Expected 'app' key indicating the exporter (might not be a sprite sheet)"), *NameForErrors);
			bLoadedSuccessfully = false;
		}

		if (OutImage.IsEmpty())
		{
			UE_LOG(LogPaperJsonImporter, Warning, TEXT("Failed to parse sprite descriptor file '%s'.  Expected valid 'image' tag"), *NameForErrors);
			bLoadedSuccessfully = false;
		}
	}
	else
	{
		UE_LOG(LogPaperJsonImporter, Warning, TEXT("Failed to parse sprite descriptor file '%s'.  Missing meta block"), *NameForErrors);
		bLoadedSuccessfully = false;
	}
	return bLoadedSuccessfully;
}

static bool ParseFrame(TSharedPtr<FJsonObject> &FrameData, FSpriteFrame &OutFrame)
{
	bool bReadFrameSuccessfully = true;
	// An example frame:
	//   "frame": {"x":210,"y":10,"w":190,"h":223},
	//   "rotated": false,
	//   "trimmed": true,
	//   "spriteSourceSize": {"x":0,"y":11,"w":216,"h":240},
	//   "sourceSize": {"w":216,"h":240},	
	//   "pivot": {"x":0.5,"y":0.5}			[optional]

	OutFrame.bRotated = FPaperJSONHelpers::ReadBoolean(FrameData, TEXT("rotated"), false);
	OutFrame.bTrimmed = FPaperJSONHelpers::ReadBoolean(FrameData, TEXT("trimmed"), false);

	bReadFrameSuccessfully = bReadFrameSuccessfully && FPaperJSONHelpers::ReadRectangle(FrameData, "frame", /*out*/ OutFrame.SpritePosInSheet, /*out*/ OutFrame.SpriteSizeInSheet);

	if (OutFrame.bTrimmed)
	{
		bReadFrameSuccessfully = bReadFrameSuccessfully && FPaperJSONHelpers::ReadSize(FrameData, "sourceSize", /*out*/ OutFrame.ImageSourceSize);
		bReadFrameSuccessfully = bReadFrameSuccessfully && FPaperJSONHelpers::ReadRectangle(FrameData, "spriteSourceSize", /*out*/ OutFrame.SpriteSourcePos, /*out*/ OutFrame.SpriteSourceSize);
	}
	else
	{
		OutFrame.SpriteSourcePos = FVector2D::ZeroVector;
		OutFrame.SpriteSourceSize = OutFrame.SpriteSizeInSheet;
		OutFrame.ImageSourceSize = OutFrame.SpriteSizeInSheet;
	}

	if (!FPaperJSONHelpers::ReadXY(FrameData, "pivot", /*out*/ OutFrame.Pivot))
	{
		OutFrame.Pivot.X = 0.5f;
		OutFrame.Pivot.Y = 0.5f;
	}

	// A few more prerequisites to sort out before rotation can be enabled
	if (OutFrame.bRotated)
	{
		// Sprite Source Pos is from top left, our pivot when rotated is bottom left
		OutFrame.SpriteSourcePos.Y = OutFrame.ImageSourceSize.Y - OutFrame.SpriteSourcePos.Y - OutFrame.SpriteSizeInSheet.Y;

		// We rotate final sprite geometry 90 deg CCW to fixup
		// These need to be swapped to be valid in texture space.
		Swap(OutFrame.SpriteSizeInSheet.X, OutFrame.SpriteSizeInSheet.Y);
		Swap(OutFrame.ImageSourceSize.X, OutFrame.ImageSourceSize.Y);

		Swap(OutFrame.SpriteSourcePos.X, OutFrame.SpriteSourcePos.Y);
		Swap(OutFrame.SpriteSourceSize.X, OutFrame.SpriteSourceSize.Y);
	}

	return bReadFrameSuccessfully;
}

static bool ParseFramesFromSpriteHash(TSharedPtr<FJsonObject> ObjectBlock, TArray<FSpriteFrame>& OutSpriteFrames, TSet<FName>& FrameNames)
{
	GWarn->BeginSlowTask(NSLOCTEXT("Paper2D", "PaperJsonImporterFactory_ParsingSprites", "Parsing Sprite Frame"), true, true);
	bool bLoadedSuccessfully = true;

	// Parse all of the frames
	int32 FrameCount = 0;
	for (auto FrameIt = ObjectBlock->Values.CreateIterator(); FrameIt; ++FrameIt)
	{
		GWarn->StatusUpdate(FrameCount, ObjectBlock->Values.Num(), NSLOCTEXT("Paper2D", "PaperJsonImporterFactory_ParsingSprites", "Parsing Sprite Frames"));

		bool bReadFrameSuccessfully = true;

		FSpriteFrame Frame;
		Frame.FrameName = *FrameIt.Key();

		if (FrameNames.Contains(Frame.FrameName))
		{
			bReadFrameSuccessfully = false;
		}
		else
		{
			FrameNames.Add(Frame.FrameName);
		}

		TSharedPtr<FJsonValue> FrameDataAsValue = FrameIt.Value();
		TSharedPtr<FJsonObject> FrameData;
		if (FrameDataAsValue->Type == EJson::Object)
		{
			FrameData = FrameDataAsValue->AsObject();
			bReadFrameSuccessfully = bReadFrameSuccessfully && ParseFrame(FrameData, /*out*/Frame);
		}
		else
		{
			bReadFrameSuccessfully = false;
		}

		if (bReadFrameSuccessfully)
		{
			OutSpriteFrames.Add(Frame);
		}
		else
		{
			UE_LOG(LogPaperJsonImporter, Warning, TEXT("Frame %s is in an unexpected format"), *Frame.FrameName.ToString());
			bLoadedSuccessfully = false;
		}

		FrameCount++;
	}

	GWarn->EndSlowTask();
	return bLoadedSuccessfully;
}

static bool ParseFramesFromSpriteArray(const TArray<TSharedPtr<FJsonValue>>& ArrayBlock, TArray<FSpriteFrame>& OutSpriteFrames, TSet<FName>& FrameNames)
{
	GWarn->BeginSlowTask(NSLOCTEXT("Paper2D", "PaperJsonImporterFactory_ParsingSprites", "Parsing Sprite Frame"), true, true);
	bool bLoadedSuccessfully = true;

	// Parse all of the frames
	int32 FrameCount = 0;
	for (int32 FrameCount = 0; FrameCount < ArrayBlock.Num(); ++FrameCount)
	{
		GWarn->StatusUpdate(FrameCount, ArrayBlock.Num(), NSLOCTEXT("Paper2D", "PaperJsonImporterFactory_ParsingSprites", "Parsing Sprite Frames"));
		bool bReadFrameSuccessfully = true;
		FSpriteFrame Frame;

		const TSharedPtr<FJsonValue>& FrameDataAsValue = ArrayBlock[FrameCount];
		if (FrameDataAsValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> FrameData;
			FrameData = FrameDataAsValue->AsObject();

			FString FrameFilename = FPaperJSONHelpers::ReadString(FrameData, TEXT("filename"), TEXT(""));
			if (!FrameFilename.IsEmpty())
			{
				Frame.FrameName = FName(*FrameFilename); // case insensitive
				if (FrameNames.Contains(Frame.FrameName))
				{
					bReadFrameSuccessfully = false;
				}
				else
				{
					FrameNames.Add(Frame.FrameName);
				}

				bReadFrameSuccessfully = bReadFrameSuccessfully && ParseFrame(FrameData, /*out*/Frame);
			}
			else
			{
				bReadFrameSuccessfully = false;
			}
		}
		else
		{
			bReadFrameSuccessfully = false;
		}

		if (bReadFrameSuccessfully)
		{
			OutSpriteFrames.Add(Frame);
		}
		else
		{
			UE_LOG(LogPaperJsonImporter, Warning, TEXT("Frame %s is in an unexpected format"), *Frame.FrameName.ToString());
			bLoadedSuccessfully = false;
		}
	}

	GWarn->EndSlowTask();
	return bLoadedSuccessfully;
}

static ESpritePivotMode::Type GetBestPivotType(FVector2D JsonPivot)
{
	// Not assuming layout of ESpritePivotMode
	if (JsonPivot.X == 0 && JsonPivot.Y == 0)
	{
		return ESpritePivotMode::Top_Left;
	}
	else if (JsonPivot.X == 0.5f && JsonPivot.Y == 0)
	{
		return ESpritePivotMode::Top_Center;
	}
	else if (JsonPivot.X == 1.0f && JsonPivot.Y == 0)
	{
		return ESpritePivotMode::Top_Right;
	}
	else if (JsonPivot.X == 0 && JsonPivot.Y == 0.5f)
	{
		return ESpritePivotMode::Center_Left;
	}
	else if (JsonPivot.X == 0.5f && JsonPivot.Y == 0.5f)
	{
		return ESpritePivotMode::Center_Center;
	}
	else if (JsonPivot.X == 1.0f && JsonPivot.Y == 0.5f)
	{
		return ESpritePivotMode::Center_Right;
	}
	else if (JsonPivot.X == 0 && JsonPivot.Y == 1.0f)
	{
		return ESpritePivotMode::Bottom_Left;
	}
	else if (JsonPivot.X == 0.5f && JsonPivot.Y == 1.0f)
	{
		return ESpritePivotMode::Bottom_Center;
	}
	else if (JsonPivot.X == 1.0f && JsonPivot.Y == 1.0f)
	{
		return ESpritePivotMode::Bottom_Right;
	}
	else
	{
		return ESpritePivotMode::Custom;
	}
}

// FPaperJsonSpriteSheetImporter
////////////////////////////////////////////

FPaperJsonSpriteSheetImporter::FPaperJsonSpriteSheetImporter() : bIsReimporting(false)
{
}

void FPaperJsonSpriteSheetImporter::SetReimportData(const FString& ExistingTextureName, UTexture2D* ExistingTexture, const TArray<FString>& ExistingSpriteNames, const TArray< TAssetPtr<class UPaperSprite> >& ExistingSpriteAssetPtrs)
{
	this->ExistingTextureName = ExistingTextureName;
	this->ExistingTexture = ExistingTexture;

	check(ExistingSpriteNames.Num() == ExistingSpriteAssetPtrs.Num());
	if (ExistingSpriteNames.Num() == ExistingSpriteAssetPtrs.Num())
	{
		for (int i = 0; i < ExistingSpriteAssetPtrs.Num(); ++i)
		{
			const TAssetPtr<class UPaperSprite> SpriteAssetPtr = ExistingSpriteAssetPtrs[i];
			FStringAssetReference SpriteStringRef = SpriteAssetPtr.ToStringReference();
			if (!SpriteStringRef.ToString().IsEmpty())
			{
				UPaperSprite* LoadedSprite = Cast<UPaperSprite>(StaticLoadObject(UPaperSprite::StaticClass(), nullptr, *SpriteStringRef.ToString(), nullptr, LOAD_None, nullptr));
				if (LoadedSprite != nullptr)
				{
					ExistingSprites.Add(ExistingSpriteNames[i], LoadedSprite);
				}
			}
		}
	}
	bIsReimporting = true;
}

bool FPaperJsonSpriteSheetImporter::Import(TSharedPtr<FJsonObject> SpriteDescriptorObject, const FString& NameForErrors)
{
	bool bLoadedSuccessfully = ParseMetaBlock(NameForErrors, SpriteDescriptorObject, Image);
	if (bLoadedSuccessfully)
	{
		TSharedPtr<FJsonObject> ObjectFrameBlock = FPaperJSONHelpers::ReadObject(SpriteDescriptorObject, TEXT("frames"));
		TSet<FName> FrameNames;

		if (ObjectFrameBlock.IsValid())
		{
			bLoadedSuccessfully = ParseFramesFromSpriteHash(ObjectFrameBlock, /*out*/Frames, /*inout*/FrameNames);
		}
		else
		{
			// Try loading as an array
			TArray<TSharedPtr<FJsonValue>> ArrayBlock = FPaperJSONHelpers::ReadArray(SpriteDescriptorObject, TEXT("frames"));
			if (ArrayBlock.Num() > 0)
			{
				bLoadedSuccessfully = ParseFramesFromSpriteArray(ArrayBlock, /*out*/Frames, /*inout*/FrameNames);
			}
			else
			{
				UE_LOG(LogPaperJsonImporter, Warning, TEXT("Failed to parse sprite descriptor file '%s'.  Missing frames block"), *NameForErrors);
				bLoadedSuccessfully = false;
			}
		}

		if (bLoadedSuccessfully && Frames.Num() == 0)
		{
			UE_LOG(LogPaperJsonImporter, Warning, TEXT("Failed to parse sprite descriptor file '%s'.  No frames loaded"), *NameForErrors);
			bLoadedSuccessfully = false;
		}
	}
	return bLoadedSuccessfully;
}

bool FPaperJsonSpriteSheetImporter::ImportFromString(const FString& FileContents, const FString& NameForErrors)
{
	TSharedPtr<FJsonObject> SpriteDescriptorObject = ParseJSON(FileContents, NameForErrors);
	return SpriteDescriptorObject.IsValid() &&
		Import(SpriteDescriptorObject, NameForErrors);
}

bool FPaperJsonSpriteSheetImporter::ImportFromArchive(FArchive* Archive, const FString& NameForErrors)
{
	TSharedPtr<FJsonObject> SpriteDescriptorObject = ParseJSON(Archive, NameForErrors);
	return SpriteDescriptorObject.IsValid() &&
		Import(SpriteDescriptorObject, NameForErrors);
}


bool FPaperJsonSpriteSheetImporter::ImportTextures(const FString& LongPackagePath, const FString& SourcePath)
{
	bool bLoadedSuccessfully = true;
	bool bFoundExistingTexture = false;
	const FString TargetSubPath = LongPackagePath + TEXT("/Textures");

	const FString SourceSheetImageFilename = FPaths::Combine(*SourcePath, *Image);
	TArray<FString> SheetFileNames;
	SheetFileNames.Add(SourceSheetImageFilename);

	if (bIsReimporting && ExistingTextureName == Image && ExistingTexture != nullptr)
	{
		ImageTexture = ExistingTexture;
		bFoundExistingTexture = true;
	}

	// Import the texture if we need to
	if (!bIsReimporting || (bIsReimporting && !bFoundExistingTexture))
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		TArray<UObject*> ImportedSheets = AssetToolsModule.Get().ImportAssets(SheetFileNames, TargetSubPath);

		UTexture2D* ImportedTexture = (ImportedSheets.Num() > 0) ? Cast<UTexture2D>(ImportedSheets[0]) : nullptr;

		if (ImportedTexture == nullptr)
		{
			UE_LOG(LogPaperJsonImporter, Warning, TEXT("Failed to import sprite sheet image '%s'."), *SourceSheetImageFilename);
			bLoadedSuccessfully = false;
		}
		else
		{
			// Change the compression settings
			ImportedTexture->Modify();
			ImportedTexture->LODGroup = TEXTUREGROUP_UI;
			ImportedTexture->CompressionSettings = TC_EditorIcon;
			ImportedTexture->Filter = TF_Nearest;
			ImportedTexture->PostEditChange();
		}

		ImageTexture = ImportedTexture;
	}

	return bLoadedSuccessfully;
}

UPaperSprite* FPaperJsonSpriteSheetImporter::FindExistingSprite(const FString& Name)
{
	UPaperSprite**ExistingSprite = ExistingSprites.Find(Name);
	if (ExistingSprite != nullptr)
	{
		return *ExistingSprite;
	}
	else
	{
		return nullptr;
	}
}

bool FPaperJsonSpriteSheetImporter::PerformImport(const FString& LongPackagePath, EObjectFlags Flags, UPaperSpriteSheet* SpriteSheet)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	GWarn->BeginSlowTask(NSLOCTEXT("Paper2D", "PaperJsonImporterFactory_ImportingSprites", "Importing Sprite Frame"), true, true);
	for (int32 FrameIndex = 0; FrameIndex < Frames.Num(); ++FrameIndex)
	{
		GWarn->StatusUpdate(FrameIndex, Frames.Num(), NSLOCTEXT("Paper2D", "PaperJsonImporterFactory_ImportingSprites", "Importing Sprite Frames"));

		// Check for user canceling the import
		if (GWarn->ReceivedUserCancel())
		{
			break;
		}

		const FSpriteFrame& Frame = Frames[FrameIndex];

		// Create a package for the frame
		const FString TargetSubPath = LongPackagePath + TEXT("/Frames");

		UObject* OuterForFrame = nullptr; // @TODO: Use this if we don't want them to be individual assets - Flipbook;

		// Create a unique package name and asset name for the frame
		const FString TentativePackagePath = PackageTools::SanitizePackageName(TargetSubPath + TEXT("/") + Frame.FrameName.ToString());
		FString DefaultSuffix;
		FString AssetName;
		FString PackageName;
		AssetToolsModule.Get().CreateUniqueAssetName(TentativePackagePath, /*out*/ DefaultSuffix, /*out*/ PackageName, /*out*/ AssetName);

		// Create a package for the frame
		OuterForFrame = CreatePackage(nullptr, *PackageName);

		// Create a frame in the package
		UPaperSprite* TargetSprite = nullptr;
		
		if (bIsReimporting)
		{
			TargetSprite = FindExistingSprite(Frame.FrameName.ToString());
		}
		
		if (TargetSprite == nullptr)
		{
			TargetSprite = NewNamedObject<UPaperSprite>(OuterForFrame, *AssetName, Flags);
			FAssetRegistryModule::AssetCreated(TargetSprite);
		}

		TargetSprite->Modify();
		FSpriteAssetInitParameters SpriteInitParams;
		SpriteInitParams.Texture = ImageTexture;
		SpriteInitParams.Offset = Frame.SpritePosInSheet;
		SpriteInitParams.Dimension = Frame.SpriteSizeInSheet;
		SpriteInitParams.bNewlyCreated = true;
		TargetSprite->InitializeSprite(SpriteInitParams);

		if (Frame.bRotated)
		{
			TargetSprite->SetRotated(true);
		}

		if (Frame.bTrimmed)
		{
			TargetSprite->SetTrim(Frame.bTrimmed, Frame.SpriteSourcePos, Frame.ImageSourceSize);
		}

		// Set up pivot on object based on Texture Packer json
		ESpritePivotMode::Type PivotType = GetBestPivotType(Frame.Pivot);
		FVector2D TextureSpacePivotPoint = FVector2D::ZeroVector;
		if (PivotType == ESpritePivotMode::Custom)
		{
			TextureSpacePivotPoint.X = Frame.SpritePosInSheet.X - Frame.SpriteSourcePos.X + Frame.ImageSourceSize.X * Frame.Pivot.X;
			TextureSpacePivotPoint.Y = Frame.SpritePosInSheet.Y - Frame.SpriteSourcePos.Y + Frame.ImageSourceSize.Y * Frame.Pivot.Y;
		}
		TargetSprite->SetPivotMode(PivotType, TextureSpacePivotPoint);

		// Create the entry in the animation
		SpriteSheet->SpriteNames.Add(Frame.FrameName.ToString());
		SpriteSheet->Sprites.Add(TargetSprite);
		
		TargetSprite->PostEditChange();
	}

	SpriteSheet->TextureName = Image;
	SpriteSheet->Texture = ImageTexture;

	GWarn->EndSlowTask();
	return true;
}
