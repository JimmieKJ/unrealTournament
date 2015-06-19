// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SpriterImporterPrivatePCH.h"
#include "PaperSpriterImportData.h"

//////////////////////////////////////////////////////////////////////////
// UPaperSpriterImportData

UPaperSpriterImportData::UPaperSpriterImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UPaperSpriterImportData::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData != nullptr)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->SourceFilePath, FAssetRegistryTag::TT_Hidden) );
	}

	Super::GetAssetRegistryTags(OutTags);
}
