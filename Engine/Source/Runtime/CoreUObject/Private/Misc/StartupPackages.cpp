// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StartupPackages.cpp: Startup Package Functions
=============================================================================*/

#include "CoreUObjectPrivate.h"
#include "Misc/StartupPackages.h"
#include "ModuleManager.h"

void FStartupPackages::GetStartupPackageNames(TArray<FString>& PackageNames, const FString& EngineConfigFilename, bool bIsCreatingHashes)
{
	// if we aren't cooking, we actually just want to use the cooked startup package as the only startup package
	if (bIsCreatingHashes || (!IsRunningCommandlet() && !GIsEditor && GUseSeekFreeLoading))
	{
		// make sure the startup package exists
		PackageNames.Add(FString(TEXT("Startup_LOC")));
		PackageNames.Add(FString(TEXT("Startup")));
	}
	else
	{
		// look for any packages that we want to force preload at startup
		FConfigSection* PackagesToPreload = GConfig->GetSectionPrivate(TEXT("Engine.StartupPackages"), 0, 1, EngineConfigFilename);
		if (PackagesToPreload)
		{
			// go through list and add to the array
			for( FConfigSectionMap::TIterator It(*PackagesToPreload); It; ++It )
			{
				if (It.Key() == TEXT("Package"))
				{
					// add this package to the list to be fully loaded later
					PackageNames.Add(*(It.Value()));
				}
			}
		}
	}
}

/**
 * Kicks off a list of packages to be read in asynchronously in the background by the
 * async file manager. The package will be serialized from RAM later.
 * 
 * @param PackageNames The list of package names to async preload
 */
static void AsyncPreloadPackageList(const TArray<FString>& PackageNames)
{
	// Iterate over all native script packages and preload them.
	for (int32 PackageIndex=0; PackageIndex<PackageNames.Num(); PackageIndex++)
	{
		// let FLinkerLoad class manage preloading
		FLinkerLoad::AsyncPreloadPackage(*PackageNames[PackageIndex]);
	}
}

void FStartupPackages::LoadPackageList(const TArray<FString>& PackageNames)
{
	// Iterate over all native script packages and fully load them.
	for( int32 PackageIndex=0; PackageIndex<PackageNames.Num(); PackageIndex++ )
	{
		UObject* Package = LoadPackage(NULL, *PackageNames[PackageIndex], LOAD_None);
	}
}

bool FStartupPackages::LoadAll()
{
	bool bReturn = true;

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading Startup Packages"), STAT_StartupPackages, STATGROUP_LoadTime);

	// should startup packages load from memory?
	bool bSerializeStartupPackagesFromMemory = false;
	GConfig->GetBool(TEXT("Engine.StartupPackages"), TEXT("bSerializeStartupPackagesFromMemory"), bSerializeStartupPackagesFromMemory, GEngineIni);

	// Get list of startup packages.
	TArray<FString> StartupPackages;
	
	// if the user wants to skip loading these, then don't (can be helpful for deleting objects in startup packages in the editor, etc)
	if (!FParse::Param(FCommandLine::Get(), TEXT("NoLoadStartupPackages")))
	{
		FStartupPackages::GetStartupPackageNames(StartupPackages);
	}

	if( bSerializeStartupPackagesFromMemory )
	{
		if( GUseSeekFreeLoading )
		{
			// kick them off to be preloaded
			AsyncPreloadPackageList(StartupPackages);
		}
	}

	return bReturn;
}
