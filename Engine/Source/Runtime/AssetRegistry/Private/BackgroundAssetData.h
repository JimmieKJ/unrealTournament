// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** A class to hold important information about an assets found by the Asset Registry. Intended for use by the background gathering thread to avoid creation of FNames */
class FBackgroundAssetData
{
public:
	/** Constructor */
	FBackgroundAssetData(const FString& InPackageName, const FString& InPackagePath, const FString& InGroupNames, const FString& InAssetName, const FString& InAssetClass, const TMap<FString, FString>& InTags, const TArray<int32>& InChunkIDs);
	FBackgroundAssetData(const FAssetData& InAssetData);

	/** Creates an AssetData object based on this object */
	FAssetData ToAssetData() const;
	
	/** The object path for the asset in the form 'Package.GroupNames.AssetName' */
	FString ObjectPath;
	/** The name of the package in which the asset is found */
	FString PackageName;
	/** The path to the package in which the asset is found */
	FString PackagePath;
	/** The '.' delimited list of group names in which the asset is found. NAME_None if there were no groups */
	FString GroupNames;
	/** The name of the asset without the package or groups */
	FString AssetName;
	/** The name of the asset's class */
	FString AssetClass;
	/** The map of values for properties that were marked AssetRegistrySearchable */
	TMap<FString, FString> TagsAndValues;
	/** The IDs of the chunks this asset is located in for streaming install.  Empty if not assigned to a chunk */
	TArray<int32> ChunkIDs;
};
