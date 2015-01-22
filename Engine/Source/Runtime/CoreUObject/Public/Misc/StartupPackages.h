// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StartupPackages.h: Startup Package Functions
=============================================================================*/

#ifndef __StartupPackages_H__
#define __StartupPackages_H__


struct FStartupPackages
{
	/**
	 * Gets the list of packages that are precached at startup for seek free loading
	 *
	 * @param PackageNames The output list of package names
	 * @param EngineConfigFilename Optional engine config filename to use to lookup the package settings
	 */
	COREUOBJECT_API static void GetStartupPackageNames(TArray<FString>& PackageNames, const FString& EngineConfigFilename=GEngineIni, bool bIsCreatingHashes=false);

	/**
	 * Fully loads a list of packages.
	 * 
	 * @param PackageNames The list of package names to load
	 */
	COREUOBJECT_API static void LoadPackageList(const TArray<FString>& PackageNames);

	/**
	 * Load all startup packages. If desired preload async followed by serialization from memory.
	 * Only native script packages are loaded from memory if we're not using the GUseSeekFreeLoading
	 * codepath as we need to reset the loader on those packages and don't want to keep the bulk
	 * data around. Native script packages don't have any bulk data so this doesn't matter.
	 *
	 * The list of additional packages can be found at Engine.StartupPackages and if seekfree loading
	 * is enabled, the startup map is going to be preloaded as well.
	 *
	 * Returns false if any modules failed to load.
	 */
	COREUOBJECT_API static bool LoadAll();
};


#endif //__StartupPackages_H__

