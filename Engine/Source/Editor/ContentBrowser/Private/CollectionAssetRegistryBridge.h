// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * The bridge between the asset registry and the collections manager - used to update collections as certain asset events happen
 */
class FCollectionAssetRegistryBridge
{
public:
	FCollectionAssetRegistryBridge();
	~FCollectionAssetRegistryBridge();

private:
	/** Called when the asset registry initial load has completed */
	void OnAssetRegistryLoadComplete();

	/** Handler for when an asset was renamed in the asset registry */
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);

	/** Handler for when an asset was removed from the asset registry */
	void OnAssetRemoved(const FAssetData& AssetData);
};
