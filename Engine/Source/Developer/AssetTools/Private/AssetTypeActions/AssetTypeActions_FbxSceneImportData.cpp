// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_SceneImportData::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto FbxSceneImportData = CastChecked<UFbxSceneImportData>(Asset);
		OutSourceFilePaths.Add(FbxSceneImportData->SourceFbxFile);
	}
}


#undef LOCTEXT_NAMESPACE
