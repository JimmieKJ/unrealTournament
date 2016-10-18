// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "PackageLocalizationManager.h"
#include "PackageLocalizationCache.h"

DEFINE_LOG_CATEGORY_STATIC(LogPackageLocalizationManager, Log, All);

class FDefaultPackageLocalizationCache : public FPackageLocalizationCache
{
public:
	virtual ~FDefaultPackageLocalizationCache() {}

protected:
	//~ FPackageLocalizationCache interface
	virtual void FindLocalizedPackages(const FString& InSourceRoot, const FString& InLocalizedRoot, TMap<FName, TArray<FName>>& InOutSourcePackagesToLocalizedPackages) override;
};

void FDefaultPackageLocalizationCache::FindLocalizedPackages(const FString& InSourceRoot, const FString& InLocalizedRoot, TMap<FName, TArray<FName>>& InOutSourcePackagesToLocalizedPackages)
{
	// Convert the package path to a filename with no extension (directory)
	FString LocalizedPackageFilePath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(InLocalizedRoot / TEXT(""), LocalizedPackageFilePath))
	{
		return;
	}

	FPackageName::IteratePackagesInDirectory(LocalizedPackageFilePath, FPackageName::FPackageNameVisitor([&](const TCHAR* InPackageFileName) -> bool
	{
		const FString PackageSubPath = FPaths::ChangeExtension(InPackageFileName + LocalizedPackageFilePath.Len(), FString());
		const FName SourcePackageName = *(InSourceRoot / PackageSubPath);
		const FName LocalizedPackageName = *(InLocalizedRoot / PackageSubPath);

		TArray<FName>& PrioritizedLocalizedPackageNames = InOutSourcePackagesToLocalizedPackages.FindOrAdd(SourcePackageName);
		PrioritizedLocalizedPackageNames.AddUnique(LocalizedPackageName);

		return true;
	}));
}

void FPackageLocalizationManager::PerformLazyInitialization()
{
	if (!ActiveCache.IsValid() && LazyInitFunc)
	{
		LazyInitFunc(*this);

		if (!ActiveCache.IsValid())
		{
			UE_LOG(LogPackageLocalizationManager, Warning, TEXT("InitializeFromLazyCallback was bound to a callback that didn't initialize the active cache."));
		}

		if (!PackageResolutionDelegate.IsBound())
		{
			PackageResolutionDelegate.BindRaw(this, &FPackageLocalizationManager::LinkerResolutionCallback);
			FCoreDelegates::PackageNameResolvers.Add(PackageResolutionDelegate);
		}
	}
}

bool FPackageLocalizationManager::LinkerResolutionCallback(const FString& InRequestedPackage, FString& OutResolvedPackage)
{
	// The editor must not redirect packages for localization. We also shouldn't redirect script or in-memory packages.
	if (GIsEditor || FPackageName::IsScriptPackage(InRequestedPackage))
	{
		return false;
	}

	const FName LocalizedPackageName = FPackageLocalizationManager::Get().FindLocalizedPackageName(*InRequestedPackage);

	if (!LocalizedPackageName.IsNone())
	{
		OutResolvedPackage = LocalizedPackageName.ToString();
	}

	return !LocalizedPackageName.IsNone();
}

void FPackageLocalizationManager::InitializeFromLazyCallback(FLazyInitFunc InLazyInitFunc)
{
	LazyInitFunc = MoveTemp(InLazyInitFunc);
	ActiveCache.Reset();
}

void FPackageLocalizationManager::InitializeFromCache(const TSharedRef<IPackageLocalizationCache>& InCache)
{
	ActiveCache = InCache;
	ActiveCache->ConditionalUpdateCache();
}

void FPackageLocalizationManager::InitializeFromDefaultCache()
{
	ActiveCache = MakeShareable(new FDefaultPackageLocalizationCache());
	ActiveCache->ConditionalUpdateCache();
}

FName FPackageLocalizationManager::FindLocalizedPackageName(const FName InSourcePackageName)
{
	PerformLazyInitialization();

	if (ActiveCache.IsValid())
	{
		return ActiveCache->FindLocalizedPackageName(InSourcePackageName);
	}
	
	UE_LOG(LogPackageLocalizationManager, Warning, TEXT("Localized package requested for '%s' before the package localization manager cache was ready. Falling back to a non-cached look-up..."), *InSourcePackageName.ToString());
	const FString CurrentCultureName = FInternationalization::Get().GetCurrentCulture()->GetName();
	return FindLocalizedPackageNameNoCache(InSourcePackageName, CurrentCultureName);
}

FName FPackageLocalizationManager::FindLocalizedPackageNameForCulture(const FName InSourcePackageName, const FString& InCultureName)
{
	PerformLazyInitialization();

	if (ActiveCache.IsValid())
	{
		return ActiveCache->FindLocalizedPackageNameForCulture(InSourcePackageName, InCultureName);
	}

	UE_LOG(LogPackageLocalizationManager, Warning, TEXT("Localized package requested for '%s' before the package localization manager cache was ready. Falling back to a non-cached look-up..."), *InSourcePackageName.ToString());
	return FindLocalizedPackageNameNoCache(InSourcePackageName, InCultureName);
}

FName FPackageLocalizationManager::FindLocalizedPackageNameNoCache(const FName InSourcePackageName, const FString& InCultureName) const
{
	// Split the package name into its root and sub-path so that we can convert it into its localized variants for testing
	FString PackageNameRoot;
	FString PackageNameSubPath;
	{
		const FString SourcePackageNameStr = InSourcePackageName.ToString();

		TArray<FString> RootPaths;
		FPackageName::QueryRootContentPaths(RootPaths);

		for (const FString& RootPath : RootPaths)
		{
			if (SourcePackageNameStr.StartsWith(RootPath, ESearchCase::IgnoreCase))
			{
				PackageNameRoot = RootPath;
				PackageNameSubPath = SourcePackageNameStr.Mid(RootPath.Len());
				break;
			}
		}
	}

	if (PackageNameRoot.IsEmpty() || PackageNameSubPath.IsEmpty())
	{
		return NAME_None;
	}

	const TArray<FString> PrioritizedCultureNames = FInternationalization::Get().GetPrioritizedCultureNames(InCultureName);
	for (const FString& PrioritizedCultureName : PrioritizedCultureNames)
	{
		const FString LocalizedPackageName = PackageNameRoot / TEXT("L10N") / PrioritizedCultureName / PackageNameSubPath;
		if (FPackageName::DoesPackageExist(LocalizedPackageName))
		{
			return *LocalizedPackageName;
		}
	}

	return NAME_None;
}

FPackageLocalizationManager& FPackageLocalizationManager::Get()
{
	static FPackageLocalizationManager PackageLocalizationManager;
	return PackageLocalizationManager;
}
