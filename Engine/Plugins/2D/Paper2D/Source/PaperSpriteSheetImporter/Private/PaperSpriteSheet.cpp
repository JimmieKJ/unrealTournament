// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PaperSpriteSheetImporterPrivatePCH.h"
#include "PaperSpriteSheet.h"

UPaperSpriteSheet::UPaperSpriteSheet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITORONLY_DATA
void UPaperSpriteSheet::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->SourceFilePath, FAssetRegistryTag::TT_Hidden) );
	}

	Super::GetAssetRegistryTags(OutTags);
}
#endif