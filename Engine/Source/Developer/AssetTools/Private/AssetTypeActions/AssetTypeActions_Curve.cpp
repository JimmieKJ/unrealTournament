// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "CurveAssetEditorModule.h"
#include "ICurveAssetEditor.h"

void FAssetTypeActions_Curve::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Curve = Cast<UCurveBase>(*ObjIt);
		if (Curve != NULL)
		{
			FCurveAssetEditorModule& CurveAssetEditorModule = FModuleManager::LoadModuleChecked<FCurveAssetEditorModule>( "CurveAssetEditor" );
			TSharedRef< ICurveAssetEditor > NewCurveAssetEditor = CurveAssetEditorModule.CreateCurveAssetEditor( Mode, EditWithinLevelEditor, Curve );
		}
	}
}

void FAssetTypeActions_Curve::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto Curve = CastChecked<UCurveBase>(Asset);
		Curve->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}