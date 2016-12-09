// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/EnginePackageLocalizationCache.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "AssetData.h"
#include "AssetRegistryModule.h"

FEnginePackageLocalizationCache::FEnginePackageLocalizationCache()
	: bIsScanningPath(false)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	AssetRegistry.OnAssetAdded().AddRaw(this, &FEnginePackageLocalizationCache::HandleAssetAdded);
	AssetRegistry.OnAssetRemoved().AddRaw(this, &FEnginePackageLocalizationCache::HandleAssetRemoved);
	AssetRegistry.OnAssetRenamed().AddRaw(this, &FEnginePackageLocalizationCache::HandleAssetRenamed);
}

FEnginePackageLocalizationCache::~FEnginePackageLocalizationCache()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		AssetRegistry.OnAssetAdded().RemoveAll(this);
		AssetRegistry.OnAssetRemoved().RemoveAll(this);
		AssetRegistry.OnAssetRenamed().RemoveAll(this);
	}
}

void FEnginePackageLocalizationCache::FindLocalizedPackages(const FString& InSourceRoot, const FString& InLocalizedRoot, TMap<FName, TArray<FName>>& InOutSourcePackagesToLocalizedPackages)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

#if WITH_EDITOR
	// Make sure the asset registry has the data we need
	{
		TArray<FString> LocalizedPackagePaths;
		LocalizedPackagePaths.Add(InLocalizedRoot);

		// Set bIsScanningPath to avoid us processing newly added assets from this scan
		bIsScanningPath = true;
		AssetRegistry.ScanPathsSynchronous(LocalizedPackagePaths);
		bIsScanningPath = false;
	}
#endif // WITH_EDITOR

	TArray<FAssetData> LocalizedAssetDataArray;
	AssetRegistry.GetAssetsByPath(*InLocalizedRoot, LocalizedAssetDataArray, /*bRecursive*/true);

	for (const FAssetData& LocalizedAssetData : LocalizedAssetDataArray)
	{
		const FName SourcePackageName = *(InSourceRoot / LocalizedAssetData.PackageName.ToString().Mid(InLocalizedRoot.Len() + 1)); // +1 for the trailing slash that isn't part of the string

		TArray<FName>& PrioritizedLocalizedPackageNames = InOutSourcePackagesToLocalizedPackages.FindOrAdd(SourcePackageName);
		PrioritizedLocalizedPackageNames.AddUnique(LocalizedAssetData.PackageName);
	}
}

void FEnginePackageLocalizationCache::HandleAssetAdded(const FAssetData& InAssetData)
{
	if (bIsScanningPath)
	{
		// Ignore this, it came from the path we're currently scanning
		return;
	}

	FScopeLock Lock(&LocalizedCachesCS);

	for (auto& CultureCachePair : AllCultureCaches)
	{
		CultureCachePair.Value->AddPackage(InAssetData.PackageName.ToString());
	}
}

void FEnginePackageLocalizationCache::HandleAssetRemoved(const FAssetData& InAssetData)
{
	FScopeLock Lock(&LocalizedCachesCS);

	for (auto& CultureCachePair : AllCultureCaches)
	{
		CultureCachePair.Value->RemovePackage(InAssetData.PackageName.ToString());
	}
}

void FEnginePackageLocalizationCache::HandleAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath)
{
	const FString OldPackagePath = FPackageName::ObjectPathToPackageName(InOldObjectPath);

	FScopeLock Lock(&LocalizedCachesCS);

	for (auto& CultureCachePair : AllCultureCaches)
	{
		CultureCachePair.Value->RemovePackage(OldPackagePath);
		CultureCachePair.Value->AddPackage(InAssetData.PackageName.ToString());
	}
}
