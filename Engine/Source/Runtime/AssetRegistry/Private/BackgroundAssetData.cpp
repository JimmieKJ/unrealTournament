// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetRegistryPCH.h"

FBackgroundAssetData::FBackgroundAssetData(const FString& InPackageName, const FString& InPackagePath, const FString& InGroupNames, const FString& InAssetName, const FString& InAssetClass, const TMap<FString, FString>& InTags, const TArray<int32>& InChunkIDs)
{
	PackageName = InPackageName;
	PackagePath = InPackagePath;
	GroupNames = InGroupNames;
	AssetName = InAssetName;
	AssetClass = InAssetClass;
	TagsAndValues = InTags;

	ObjectPath = PackageName + TEXT(".");

	if ( GroupNames.Len() )
	{
		ObjectPath += GroupNames + TEXT(".");
	}

	ObjectPath += AssetName;

	ChunkIDs = InChunkIDs;
}

FBackgroundAssetData::FBackgroundAssetData(const FAssetData& InAssetData)
{
	PackageName = InAssetData.PackageName.ToString();
	PackagePath = InAssetData.PackagePath.ToString();
	GroupNames = InAssetData.GroupNames.ToString();
	AssetName = InAssetData.AssetName.ToString();
	AssetClass = InAssetData.AssetClass.ToString();

	for ( auto TagIt = InAssetData.TagsAndValues.CreateConstIterator(); TagIt; ++TagIt )
	{
		TagsAndValues.Add(TagIt.Key().ToString(), TagIt.Value());
	}

	ObjectPath = InAssetData.ObjectPath.ToString();

	ChunkIDs = InAssetData.ChunkIDs;
}

FAssetData FBackgroundAssetData::ToAssetData() const
{
	TMap<FName, FString> CopiedTagsAndValues;
	for (TMap<FString,FString>::TConstIterator TagIt(TagsAndValues); TagIt; ++TagIt)
	{
		CopiedTagsAndValues.Add(FName(*TagIt.Key()), TagIt.Value());
	}

	return FAssetData(
		FName(*PackageName),
		FName(*PackagePath),
		FName(*GroupNames),
		FName(*AssetName),
		FName(*AssetClass),
		CopiedTagsAndValues,
		ChunkIDs
		);
}
