// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookCommandlet.cpp: Commandlet for cooking content
=============================================================================*/

#pragma once
#include "Commandlets/Commandlet.h"
#include "CookCommandlet.generated.h"

UCLASS(config=Editor)
class UCookCommandlet
	: public UCommandlet
{
	GENERATED_UCLASS_BODY()

	/** List of asset types that will force GC after loading them during cook */
	UPROPERTY(config)
	TArray<FString> FullGCAssetClassNames;

	/** If true, iterative cooking is being done */
	bool bIterativeCooking;
	/** If true, packages are cooked compressed */
	bool bCompressed;
	/** Prototype cook-on-the-fly server */
	bool bCookOnTheFly; 
	/** Cook everything */
	bool bCookAll;
	/** Skip saving any packages in Engine/COntent/Editor* UNLESS TARGET HAS EDITORONLY DATA (in which case it will save those anyway) */
	bool bSkipEditorContent;
	/** Test for UObject leaks */
	bool bLeakTest;  
	/** Save all cooked packages without versions. These are then assumed to be current version on load. This is dangerous but results in smaller patch sizes. */
	bool bUnversioned;
	/** Generate manifests for building streaming install packages */
	bool bGenerateStreamingInstallManifests;
	/** Error if we access engine content (useful for dlc) */
	bool bErrorOnEngineContentUse;
	/** Use historical serialization system for generating package dependencies (use for historical reasons only this method has been depricated, only affects cooked manifests) */
	bool bUseSerializationForGeneratingPackageDependencies;
	/** Only cook packages specified on commandline options (for debugging)*/
	bool bCookSinglePackage;
	/** Should we output additional verbose cooking warnings */
	bool bVerboseCookerWarnings;
	/** only clean up objects which are not in use by the cooker when we gc (false will enable full gc) */
	bool bPartialGC;
	/** All commandline tokens */
	TArray<FString> Tokens;
	/** All commandline switches */
	TArray<FString> Switches;
	/** All commandline params */
	FString Params;

	/**
	 * Cook on the fly routing for the commandlet
	 *
	 * @param  BindAnyPort					Whether to bind on any port or the default port.
	 * @param  Timeout						Length of time to wait for connections before attempting to close
	 * @param  bForceClose					Whether or not the server should always shutdown after a timeout or after a user disconnects
	 *
	 * @return true on success, false otherwise.
	 */
	bool CookOnTheFly( FGuid InstanceId, int32 Timeout = 180, bool bForceClose = false );

	/**
	 * Returns cooker output directory.
	 *
	 * @param PlatformName Target platform name.
	 * @return Cooker output directory for the specified platform.
	 */
	FString GetOutputDirectory( const FString& PlatformName ) const;

	/**
	 *	Get the given packages 'cooked' timestamp (i.e. account for dependencies)
	 *
	 *	@param	InFilename			The filename of the package
	 *	@param	OutDateTime			The timestamp the cooked file should have
	 *
	 *	@return	bool				true if the package timestamp was found, false if not
	 */
	bool GetPackageTimestamp( const FString& InFilename, FDateTime& OutDateTime );

	/**
	 *	Cook (save) the given package
	 *
	 *	@param	Package				The package to cook/save
	 *	@param	SaveFlags			The flags to pass to the SavePackage function
	 *	@param	bOutWasUpToDate		Upon return, if true then the cooked package was cached (up to date)
	 *
	 *	@return	bool			true if packages was cooked
	 */
	bool SaveCookedPackage( UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate );
	/**
	 *	Cook (save) the given package
	 *
	 *	@param	Package				The package to cook/save
	 *	@param	SaveFlags			The flags to pass to the SavePackage function
	 *	@param	bOutWasUpToDate		Upon return, if true then the cooked package was cached (up to date)
	 *	@param  TargetPlatformNames Only cook for target platforms which are included in this array (if empty cook for all target platforms specified on commandline options)
	 *									TargetPlatformNames is in and out value returns the platforms which the SaveCookedPackage function saved for
	 *
	 *	@return	bool			true if packages was cooked
	 */
	bool SaveCookedPackage( UPackage* Package, uint32 SaveFlags, bool& bOutWasUpToDate, TArray<FString> &TargetPlatformNames );

	bool ShouldCook(const FString& InFilename, const FString &InPlatformname = TEXT(""));

public:

	//~ Begin UCommandlet Interface

	virtual int32 Main(const FString& CmdLineParams) override;
	
	//~ End UCommandlet Interface

private:

	/** Holds the sandbox file wrapper to handle sandbox path conversion. */
	TAutoPtr<class FSandboxPlatformFile> SandboxFile;

	/** We hook this up to a delegate to avoid reloading textures and whatnot */
	TSet<FString> PackagesToNotReload;

	/** Leak test: last gc items */
	TSet<FWeakObjectPtr> LastGCItems;

	void MaybeMarkPackageAsAlreadyLoaded(UPackage *Package);

	/** See if the cooker has exceeded max memory allowance in this case the cooker should force a garbage collection */
	bool HasExceededMaxMemory(uint64 MaxMemoryAllowance) const;

	/** Gets the output directory respecting any command line overrides */
	FString GetOutputDirectoryOverride() const;

	/** Cleans sandbox folders for all target platforms */
	void CleanSandbox(const TArray<ITargetPlatform*>& Platforms);

	/** Generates asset registry */
	void GenerateAssetRegistry(const TArray<ITargetPlatform*>& Platforms);

	/** Saves global shader map files */
	void SaveGlobalShaderMapFiles(const TArray<ITargetPlatform*>& Platforms);

	/** Collects all files to be cooked. This includes all commandline specified maps */
	void CollectFilesToCook(TArray<FString>& FilesInPath);

	/** Generates long package names for all files to be cooked */
	void GenerateLongPackageNames(TArray<FString>& FilesInPath);

	/** Cooks all files */
	bool Cook(const TArray<ITargetPlatform*>& Platforms, TArray<FString>& FilesInPath);

	/** Cooks all files newly (in a new way) */
	bool NewCook(const TArray<ITargetPlatform*>& Platforms, TArray<FString>& FilesInPath);


	/**	Process deferred commands */
	void ProcessDeferredCommands();

	/** Adds a unique package filename to cook. Rejects script packages. */
	FORCEINLINE void AddFileToCook(TArray<FString>& InOutFilesToCook, const FString& InFilename) const
	{
		if (!FPackageName::IsScriptPackage(InFilename))
		{
			InOutFilesToCook.AddUnique(InFilename);
		}
	}
};