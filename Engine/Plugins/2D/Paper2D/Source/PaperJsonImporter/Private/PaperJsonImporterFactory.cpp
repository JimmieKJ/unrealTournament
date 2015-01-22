// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PaperJsonImporterPrivatePCH.h"
#include "Paper2DClasses.h"
#include "Json.h"
#include "PaperJSONHelpers.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "PackageTools.h"
#include "PaperSpriteSheet.h"
#include "PaperJsonSpriteSheetImporter.h"
#include "ReimportPaperJsonImporterFactory.h"

//////////////////////////////////////////////////////////////////////////
// UPaperJsonImporterFactory

UPaperJsonImporterFactory::UPaperJsonImporterFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	//bEditAfterNew = true;
	SupportedClass = UPaperSpriteSheet::StaticClass();

	bEditorImport = true;
	bText = true;

	Formats.Add(TEXT("json;Spritesheet JSON file"));
	Formats.Add(TEXT("paper2dsprites;Spritesheet JSON file"));
}

FText UPaperJsonImporterFactory::GetToolTip() const
{
	return NSLOCTEXT("Paper2D", "PaperJsonImporterFactoryDescription", "Sprite sheets exported from Adobe Flash");
}

UObject* UPaperJsonImporterFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	Flags |= RF_Transactional;

	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	const FString CurrentFilename = UFactory::GetCurrentFilename();
	FString CurrentSourcePath;
	FString FilenameNoExtension;
	FString UnusedExtension;
	FPaths::Split(CurrentFilename, CurrentSourcePath, FilenameNoExtension, UnusedExtension);

	

	const FString LongPackagePath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetPathName());
	UPaperSpriteSheet* Result = nullptr;
	
	const FString NameForErrors(InName.ToString());
	const FString FileContent(BufferEnd - Buffer, Buffer);
	
	FPaperJsonSpriteSheetImporter Importer;

	// Are we reimporting?
	if (IsA(UReimportPaperJsonImporterFactory::StaticClass()))
	{
		UReimportPaperJsonImporterFactory* CurrentReimportFactory = Cast<UReimportPaperJsonImporterFactory>(this);
		Importer.SetReimportData(CurrentReimportFactory->ExistingTextureName, CurrentReimportFactory->ExistingTexture, CurrentReimportFactory->ExistingSpriteNames, CurrentReimportFactory->ExistingSprites);
	}

	if (Importer.ImportFromString(FileContent, NameForErrors) &&
		Importer.ImportTextures(LongPackagePath, CurrentSourcePath))
	{
		UPaperSpriteSheet* SpriteSheet = NewNamedObject<UPaperSpriteSheet>(InParent, InName, Flags);
		if (Importer.PerformImport(LongPackagePath, Flags, SpriteSheet))
		{
			if (SpriteSheet->AssetImportData == nullptr)
			{
				SpriteSheet->AssetImportData = ConstructObject<UAssetImportData>(UAssetImportData::StaticClass(), SpriteSheet);
			}

			SpriteSheet->AssetImportData->SourceFilePath = FReimportManager::SanitizeImportFilename(CurrentFilename, SpriteSheet);
			SpriteSheet->AssetImportData->SourceFileTimestamp = IFileManager::Get().GetTimeStamp(*CurrentFilename).ToString();
			SpriteSheet->AssetImportData->bDirty = false;

			Result = SpriteSheet;
		}
	}
	
	FEditorDelegates::OnAssetPostImport.Broadcast(this, Result);

	return Result;
}
