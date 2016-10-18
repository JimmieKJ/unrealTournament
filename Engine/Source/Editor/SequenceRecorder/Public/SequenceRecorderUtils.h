// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistryModule.h"

namespace SequenceRecorderUtils
{

/**
 * Utility function that creates an asset with the specified asset path and name.
 * If the asset cannot be created (as one already exists), we try to postfix the asset
 * name until we can successfully create the asset.
 */
template<typename AssetType>
static AssetType* MakeNewAsset(const FString& BaseAssetPath, const FString& BaseAssetName)
{
	const FString Dot(TEXT("."));
	FString AssetPath = BaseAssetPath;
	FString AssetName = BaseAssetName;

	AssetPath /= AssetName;
	AssetPath += Dot + AssetName;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetPath);

	// if object with same name exists, try a different name until we don't find one
	int32 ExtensionIndex = 0;
	while(AssetData.IsValid())
	{
		AssetName = FString::Printf(TEXT("%s_%d"), *BaseAssetName, ExtensionIndex);
		AssetPath = (BaseAssetPath / AssetName) + Dot + AssetName;
		AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetPath);

		ExtensionIndex++;
	}

	// Create the new asset in the package we just made
	AssetPath = (BaseAssetPath / AssetName);

	FString FileName;
	if(FPackageName::TryConvertLongPackageNameToFilename(AssetPath, FileName))
	{
		UObject* Package = CreatePackage(nullptr, *AssetPath);
		return NewObject<AssetType>(Package, *AssetName, RF_Public | RF_Standalone);	
	}

	UE_LOG(LogAnimation, Error, TEXT("Couldnt create file for package %s"), *AssetPath);

	return nullptr;
}

template<typename AssetType>
static FString MakeNewAssetName(const FString& BaseAssetPath, const FString& BaseAssetName)
{
	const FString Dot(TEXT("."));
	FString AssetPath = BaseAssetPath;
	FString AssetName = BaseAssetName;

	AssetPath /= AssetName;
	AssetPath += Dot + AssetName;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetPath);

	// if object with same name exists, try a different name until we don't find one
	int32 ExtensionIndex = 0;
	while (AssetData.IsValid())
	{
		AssetName = FString::Printf(TEXT("%s_%d"), *BaseAssetName, ExtensionIndex);
		AssetPath = (BaseAssetPath / AssetName) + Dot + AssetName;
		AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetPath);

		ExtensionIndex++;
	}

	return AssetName;
}

/** Helper function - check whether our component hierarchy has some attachment outside of its owned components */
SEQUENCERECORDER_API AActor* GetAttachment(AActor* InActor, FName& SocketName, FName& ComponentName);

}