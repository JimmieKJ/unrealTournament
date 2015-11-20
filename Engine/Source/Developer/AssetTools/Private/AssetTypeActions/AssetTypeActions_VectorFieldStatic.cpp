// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"

void FAssetTypeActions_VectorFieldStatic::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto StaticVectorField = CastChecked<UVectorFieldStatic>(Asset);
		StaticVectorField->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}