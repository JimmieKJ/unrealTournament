// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FNewAssetContextMenu
{
public:
	DECLARE_DELEGATE_TwoParams( FOnNewAssetRequested, const FString& /*SelectedPath*/, TWeakObjectPtr<UClass> /*FactoryClass*/ );
	DECLARE_DELEGATE_OneParam( FOnNewFolderRequested, const FString& /*SelectedPath*/ );

	/** Makes the context menu widget */
	static void MakeContextMenu(FMenuBuilder& MenuBuilder, const FString& InPath, const FOnNewAssetRequested& InOnNewAssetRequested, const FOnNewFolderRequested& InOnNewFolderRequested);

private:
	/** Handle creating a new asset from an asset category */
	static void CreateNewAssetMenuCategory(FMenuBuilder& MenuBuilder, EAssetTypeCategories::Type AssetTypeCategory, FString InPath, FOnNewAssetRequested InOnNewAssetRequested);

	/** Returns true if InPath is valid */
	static bool IsAssetPathSelected(FString InPath);

	/** Handle when the "Import" button is clicked */
	static void ExecuteImportAsset( FString InPath );

	/** Create a new asset using the specified factory at the specified path */
	static void ExecuteNewAsset(FString InPath, TWeakObjectPtr<UClass> FactoryClass, FOnNewAssetRequested InOnNewAssetRequested);

	/** Create a new folder at the specified path */
	static void ExecuteNewFolder(FString InPath, FOnNewFolderRequested InOnNewFolderRequested);
};
