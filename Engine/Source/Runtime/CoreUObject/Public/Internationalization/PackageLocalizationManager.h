// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class IPackageLocalizationCache;

/** Singleton class that manages localized package data. */
class COREUOBJECT_API FPackageLocalizationManager
{
public:
	typedef TFunction<void(FPackageLocalizationManager&)> FLazyInitFunc;

	/**
	 * Initialize the manager from the callback set by InitializeFromLazyCallback. It is expected that this callback calls one of the InitializeFromX functions.
	 */
	void PerformLazyInitialization();
	
	/**
	 * Initialize the manager lazily using the given callback. It is expected that this callback calls one of the InitializeFromX functions.
	 *
	 * @param InLazyInitFunc	The function to call to initialize the manager.
	 */
	void InitializeFromLazyCallback(FLazyInitFunc InLazyInitFunc);

	/**
	 * Initialize the manager using the given cache. This will perform an initial scan for localized packages.
	 *
	 * @param InCache	The cache the manager should use.
	 */
	void InitializeFromCache(const TSharedRef<IPackageLocalizationCache>& InCache);

	/**
	 * Initialize this manager using the default cache. This will perform an initial scan for localized packages.
	 */
	void InitializeFromDefaultCache();

	/**
	 * Try and find the localized package name for the given source package for the active culture.
	 *
	 * @param InSourcePackageName	The name of the source package to find.
	 *
	 * @return The localized package name, or NAME_None if there is no localized package.
	 */
	FName FindLocalizedPackageName(const FName InSourcePackageName);

	/**
	 * Try and find the localized package name for the given source package for the given culture.
	 *
	 * @param InSourcePackageName	The name of the source package to find.
	 * @param InCultureName			The name of the culture to find the package for.
	 *
	 * @return The localized package name, or NAME_None if there is no localized package.
	 */
	FName FindLocalizedPackageNameForCulture(const FName InSourcePackageName, const FString& InCultureName);

	/**
	 * Singleton accessor.
	 *
	 * @return The singleton instance of the localization manager.
	 */
	static FPackageLocalizationManager& Get();

private:
	/**
	 * Try and find the localized package name for the given source package for the given culture, but without going through the cache.
	 *
	 * @param InSourcePackageName	The name of the source package to find.
	 * @param InCultureName			The name of the culture to find the package for.
	 *
	 * @return The localized package name, or NAME_None if there is no localized package.
	 */
	FName FindLocalizedPackageNameNoCache(const FName InSourcePackageName, const FString& InCultureName) const;

	/**
	* Delegate registered with FCoreDelegates to allow us to perform resolution of package names
	*
	* @param InRequestedPackage		The name of the package requested for load.
	* @param OutResolvedPackage		The name of the resolved package to actually load
	*
	* @return						True/False based on whether resolution occured
	*/
	bool LinkerResolutionCallback(const FString& InRequestedPackage, FString& OutResolvedPackage);

	/** Function to call to lazily initialize the manager. */
	FLazyInitFunc LazyInitFunc;

	/** Pointer to our currently active cache. Only valid after Initialize has been called. */
	TSharedPtr<IPackageLocalizationCache> ActiveCache;

	/** Delegate installed to handle resolution of packages during loading */
	FCoreDelegates::FResolvePackageNameDelegate  PackageResolutionDelegate;
};
