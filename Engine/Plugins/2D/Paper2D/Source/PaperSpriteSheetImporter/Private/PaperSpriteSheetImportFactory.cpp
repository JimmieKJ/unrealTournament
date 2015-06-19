// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PaperSpriteSheetImporterPrivatePCH.h"
#include "Json.h"
#include "PaperJSONHelpers.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "PackageTools.h"
#include "PaperSpriteSheet.h"
#include "PaperSpriteSheetImportFactory.h"
#include "PaperSpriteSheetReimportFactory.h"

//////////////////////////////////////////////////////////////////////////
// UPaperSpriteSheetImportFactory

UPaperSpriteSheetImportFactory::UPaperSpriteSheetImportFactory(const FObjectInitializer& ObjectInitializer)
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

FText UPaperSpriteSheetImportFactory::GetToolTip() const
{
	return NSLOCTEXT("Paper2D", "PaperJsonImporterFactoryDescription", "Sprite sheets exported from Adobe Flash or Texture Packer");
}

UObject* UPaperSpriteSheetImportFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
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
	
	if (Importer.ImportFromString(FileContent, NameForErrors) &&
		Importer.ImportTextures(LongPackagePath, CurrentSourcePath))
	{
		UPaperSpriteSheet* SpriteSheet = NewObject<UPaperSpriteSheet>(InParent, InName, Flags);
		if (Importer.PerformImport(LongPackagePath, Flags, SpriteSheet))
		{
			if (SpriteSheet->AssetImportData == nullptr)
			{
				SpriteSheet->AssetImportData = NewObject<UAssetImportData>(SpriteSheet);
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
