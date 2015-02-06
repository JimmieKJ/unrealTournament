// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PaperJsonImporterPrivatePCH.h"
#include "ReimportPaperJsonImporterFactory.h"
#include "PaperSpriteSheet.h"

#define LOCTEXT_NAMESPACE "PaperJsonImporter"


UReimportPaperJsonImporterFactory::UReimportPaperJsonImporterFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPaperSpriteSheet::StaticClass();

	bCreateNew = false;
	bText = true;
}

bool UReimportPaperJsonImporterFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UPaperSpriteSheet* SpriteSheet = Cast<UPaperSpriteSheet>(Obj);
	if (SpriteSheet && SpriteSheet->AssetImportData)
	{
		OutFilenames.Add(FReimportManager::ResolveImportFilename(SpriteSheet->AssetImportData->SourceFilePath, SpriteSheet));
		return true;
	}
	return false;
}

void UReimportPaperJsonImporterFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UPaperSpriteSheet* SpriteSheet = Cast<UPaperSpriteSheet>(Obj);
	if (SpriteSheet && ensure(NewReimportPaths.Num() == 1))
	{
		SpriteSheet->AssetImportData->SourceFilePath = FReimportManager::ResolveImportFilename(NewReimportPaths[0], SpriteSheet);
	}
}

EReimportResult::Type UReimportPaperJsonImporterFactory::Reimport(UObject* Obj)
{
	UPaperSpriteSheet* SpriteSheet = Cast<UPaperSpriteSheet>(Obj);
	if (!SpriteSheet)
	{
		return EReimportResult::Failed;
	}

	// Make sure file is valid and exists
	const FString Filename = FReimportManager::ResolveImportFilename(SpriteSheet->AssetImportData->SourceFilePath, SpriteSheet);
	if (!Filename.Len() || IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}

	ExistingSpriteNames = SpriteSheet->SpriteNames;
	ExistingSprites = SpriteSheet->Sprites;
	ExistingTextureName = SpriteSheet->TextureName;
	ExistingTexture = SpriteSheet->Texture;

	EReimportResult::Type Result = EReimportResult::Failed;
	if (UFactory::StaticImportObject(SpriteSheet->GetClass(), SpriteSheet->GetOuter(), *SpriteSheet->GetName(), RF_Public | RF_Standalone, *Filename, nullptr, this))
	{
		UE_LOG(LogPaperJsonImporter, Log, TEXT("Imported successfully"));
		// Try to find the outer package so we can dirty it up
		if (SpriteSheet->GetOuter())
		{
			SpriteSheet->GetOuter()->MarkPackageDirty();
		}
		else
		{
			SpriteSheet->MarkPackageDirty();
		}
		Result = EReimportResult::Succeeded;
	}
	else
	{
		UE_LOG(LogPaperJsonImporter, Warning, TEXT("-- import failed"));
		Result = EReimportResult::Failed;
	}

	return Result;
}

int32 UReimportPaperJsonImporterFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE
