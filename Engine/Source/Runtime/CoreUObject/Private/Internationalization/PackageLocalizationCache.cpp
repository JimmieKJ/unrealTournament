// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "PackageLocalizationCache.h"

DEFINE_LOG_CATEGORY_STATIC(LogPackageLocalizationCache, Log, All);

FPackageLocalizationCultureCache::FPackageLocalizationCultureCache(FPackageLocalizationCache* InOwnerCache, const FString& InCultureName)
	: OwnerCache(InOwnerCache)
{
	PrioritizedCultureNames = FInternationalization::Get().GetPrioritizedCultureNames(InCultureName);
}

void FPackageLocalizationCultureCache::ConditionalUpdateCache()
{
	FScopeLock Lock(&LocalizedPackagesCS);
	ConditionalUpdateCache_NoLock();
}

void FPackageLocalizationCultureCache::ConditionalUpdateCache_NoLock()
{
	if (PendingSourceRootPathsToSearch.Num() == 0)
	{
		return;
	}

	const double CacheStartTime = FPlatformTime::Seconds();

	for (const FString& SourceRootPath : PendingSourceRootPathsToSearch)
	{
		TArray<FString>& LocalizedRootPaths = SourcePathsToLocalizedPaths.FindOrAdd(SourceRootPath);
		for (const FString& PrioritizedCultureName : PrioritizedCultureNames)
		{
			const FString LocalizedRootPath = SourceRootPath / TEXT("L10N") / PrioritizedCultureName;
			if (!LocalizedRootPaths.Contains(LocalizedRootPath))
			{
				LocalizedRootPaths.Add(LocalizedRootPath);
				OwnerCache->FindLocalizedPackages(SourceRootPath, LocalizedRootPath, SourcePackagesToLocalizedPackages);
			}
		}
	}

	UE_LOG(LogPackageLocalizationCache, Log, TEXT("Processed %d localized package path(s) for %d prioritized culture(s) in %0.6f seconds"), PendingSourceRootPathsToSearch.Num(), PrioritizedCultureNames.Num(), FPlatformTime::Seconds() - CacheStartTime);

	PendingSourceRootPathsToSearch.Empty();
}

void FPackageLocalizationCultureCache::AddRootSourcePath(const FString& InRootPath)
{
	FScopeLock Lock(&LocalizedPackagesCS);

	// Add this to the list of paths to process - it will get picked up the next time the cache is updated
	PendingSourceRootPathsToSearch.AddUnique(InRootPath);
}

void FPackageLocalizationCultureCache::RemoveRootSourcePath(const FString& InRootPath)
{
	FScopeLock Lock(&LocalizedPackagesCS);

	// Remove it from the pending list
	PendingSourceRootPathsToSearch.Remove(InRootPath);

	// Remove all paths under this root
	for (auto It = SourcePathsToLocalizedPaths.CreateIterator(); It; ++It)
	{
		if (It->Key.StartsWith(InRootPath))
		{
			It.RemoveCurrent();
			continue;
		}
	}

	// Remove all packages under this root
	for (auto It = SourcePackagesToLocalizedPackages.CreateIterator(); It; ++It)
	{
		if (It->Key.ToString().StartsWith(InRootPath))
		{
			It.RemoveCurrent();
			continue;
		}
	}
}

void FPackageLocalizationCultureCache::AddPackage(const FString& InPackageName)
{
	if (!FPackageName::IsLocalizedPackage(InPackageName))
	{
		return;
	}

	FScopeLock Lock(&LocalizedPackagesCS);

	// Is this package for a localized path that we care about
	for (const auto& SourcePathToLocalizedPathsPair : SourcePathsToLocalizedPaths)
	{
		for (const FString& LocalizedRootPath : SourcePathToLocalizedPathsPair.Value)
		{
			if (InPackageName.StartsWith(LocalizedRootPath, ESearchCase::IgnoreCase))
			{
				const FName SourcePackageName = *(SourcePathToLocalizedPathsPair.Key / InPackageName.Mid(LocalizedRootPath.Len() + 1)); // +1 for the trailing slash that isn't part of the string

				TArray<FName>& PrioritizedLocalizedPackageNames = SourcePackagesToLocalizedPackages.FindOrAdd(SourcePackageName);
				PrioritizedLocalizedPackageNames.AddUnique(*InPackageName);
				return;
			}
		}
	}
}

void FPackageLocalizationCultureCache::RemovePackage(const FString& InPackageName)
{
	FScopeLock Lock(&LocalizedPackagesCS);

	if (FPackageName::IsLocalizedPackage(InPackageName))
	{
		const FName LocalizedPackageName = *InPackageName;

		// Try and find the corresponding localized package to remove
		// If the package was the last localized package for a source package, then we remove the whole mapping entry
		for (auto& SourcePackageToLocalizedPackagesPair : SourcePackagesToLocalizedPackages)
		{
			TArray<FName>& PrioritizedLocalizedPackageNames = SourcePackageToLocalizedPackagesPair.Value;
			if (PrioritizedLocalizedPackageNames.Remove(LocalizedPackageName) > 0)
			{
				if (PrioritizedLocalizedPackageNames.Num() == 0)
				{
					SourcePackagesToLocalizedPackages.Remove(SourcePackageToLocalizedPackagesPair.Key);
				}
				return;
			}
		}
	}
	else
	{
		SourcePackagesToLocalizedPackages.Remove(*InPackageName);
	}
}

void FPackageLocalizationCultureCache::Empty()
{
	FScopeLock Lock(&LocalizedPackagesCS);

	PendingSourceRootPathsToSearch.Empty();
	SourcePathsToLocalizedPaths.Empty();
	SourcePackagesToLocalizedPackages.Empty();
}

FName FPackageLocalizationCultureCache::FindLocalizedPackageName(const FName InSourcePackageName)
{
	FScopeLock Lock(&LocalizedPackagesCS);

	ConditionalUpdateCache_NoLock();

	const TArray<FName>* const FoundPrioritizedLocalizedPackageNames = SourcePackagesToLocalizedPackages.Find(InSourcePackageName);
	return (FoundPrioritizedLocalizedPackageNames) ? (*FoundPrioritizedLocalizedPackageNames)[0] : NAME_None;
}

FPackageLocalizationCache::FPackageLocalizationCache()
{
	HandleCultureChanged();
	FInternationalization::Get().OnCultureChanged().AddRaw(this, &FPackageLocalizationCache::HandleCultureChanged);

	FPackageName::OnContentPathMounted().AddRaw(this, &FPackageLocalizationCache::HandleContentPathMounted);
	FPackageName::OnContentPathDismounted().AddRaw(this, &FPackageLocalizationCache::HandleContentPathDismounted);
}

FPackageLocalizationCache::~FPackageLocalizationCache()
{
	if (FInternationalization::IsAvailable())
	{
		FInternationalization::Get().OnCultureChanged().RemoveAll(this);
	}

	FPackageName::OnContentPathMounted().RemoveAll(this);
	FPackageName::OnContentPathDismounted().RemoveAll(this);
}

void FPackageLocalizationCache::ConditionalUpdateCache()
{
	FScopeLock Lock(&LocalizedCachesCS);

	for (auto& CultureCachePair : AllCultureCaches)
	{
		CultureCachePair.Value->ConditionalUpdateCache();
	}
}

FName FPackageLocalizationCache::FindLocalizedPackageName(const FName InSourcePackageName)
{
	FScopeLock Lock(&LocalizedCachesCS);

	return (CurrentCultureCache.IsValid()) ? CurrentCultureCache->FindLocalizedPackageName(InSourcePackageName) : NAME_None;
}

FName FPackageLocalizationCache::FindLocalizedPackageNameForCulture(const FName InSourcePackageName, const FString& InCultureName)
{
	FScopeLock Lock(&LocalizedCachesCS);

	TSharedPtr<FPackageLocalizationCultureCache> CultureCache = FindOrAddCacheForCulture_NoLock(InCultureName);
	return (CultureCache.IsValid()) ? CultureCache->FindLocalizedPackageName(InSourcePackageName) : NAME_None;
}

TSharedPtr<FPackageLocalizationCultureCache> FPackageLocalizationCache::FindOrAddCacheForCulture_NoLock(const FString& InCultureName)
{
	if (InCultureName.IsEmpty())
	{
		return nullptr;
	}

	TSharedPtr<FPackageLocalizationCultureCache>& CultureCache = AllCultureCaches.FindOrAdd(InCultureName);
	if (!CultureCache.IsValid())
	{
		CultureCache = MakeShareable(new FPackageLocalizationCultureCache(this, InCultureName));
		
		// Add the current set of root paths
		TArray<FString> RootPaths;
		FPackageName::QueryRootContentPaths(RootPaths);
		for (const FString& RootPath : RootPaths)
		{
			CultureCache->AddRootSourcePath(RootPath);
		}
	}

	return CultureCache;
}

void FPackageLocalizationCache::HandleContentPathMounted(const FString& InAssetPath, const FString& InFilesystemPath)
{
	FScopeLock Lock(&LocalizedCachesCS);

	for (auto& CultureCachePair : AllCultureCaches)
	{
		CultureCachePair.Value->AddRootSourcePath(InAssetPath);
	}
}

void FPackageLocalizationCache::HandleContentPathDismounted(const FString& InAssetPath, const FString& InFilesystemPath)
{
	FScopeLock Lock(&LocalizedCachesCS);

	for (auto& CultureCachePair : AllCultureCaches)
	{
		CultureCachePair.Value->RemoveRootSourcePath(InAssetPath);
	}
}

void FPackageLocalizationCache::HandleCultureChanged()
{
	FScopeLock Lock(&LocalizedCachesCS);

	// Clear out all current caches and re-scan for the current culture
	CurrentCultureCache.Reset();
	AllCultureCaches.Empty();

	const FString CurrentCultureName = FInternationalization::Get().GetCurrentCulture()->GetName();
	CurrentCultureCache = FindOrAddCacheForCulture_NoLock(CurrentCultureName);
}
