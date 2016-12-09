// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FPackageDependencyTrackingInfo;

/**
 *	Package dependency information class.
 *	Generates and maintains a list of dependency and timestamp information for packages.
 *	Primarily intended for use during cooking.
 */
class FPackageDependencyInfo
{
	friend class FPackageDependencyInfoModule;

public:
	static FString ScriptSourcePkgName;
	static FString ShaderSourcePkgName;

	FPackageDependencyInfo() {};
	~FPackageDependencyInfo();

	/** 
	 *	Initialize the class
	 *
	 *	@param	bInPreProcessAllFiles		If true, pre-process all content files at initialization
	 */
	void Initialize(bool bInPreProcessAllFiles);

	/**
	 *	Determine the given packages dependent time stamp
	 *
	 *	@param	InPackageName		The package to process
	 *	@param	OutNewestTime		The dependent time stamp for the package.
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool DeterminePackageDependentTimeStamp(const TCHAR* InPackageName, FDateTime& OutNewestTime);




	/**
	 *	Determine the given packages dependent hash
	 *
	 *	@param	InPackageName		The package to process
	 *	@param	OutDependentHash	The hash of this package and dependent packages 
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool DeterminePackageDependentHash(const TCHAR* InPackageName, FMD5Hash& OutDependentHash);

	/**
 	 *	Determine the given packages file full hash
	 *
	 *	@param	InPackageName		The package to process
	 *	@param	OutNewestTime		The dependent time stamp for the package.
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool DetermineFullPackageHash(const TCHAR* InPackageName, FMD5Hash& OutFullPackageHash);

	/**
	 *	Determine dependent timestamps for the given list of files
	 *
	 *	@param	InPackageList		The list of packages to process
	 */
	void DetermineDependentTimeStamps(const TArray<FString>& InPackageList);

	/**
	 *	Determine all found content files dependent timestamps
	 */
	void DetermineAllDependentTimeStamps();

	void AsyncDetermineAllDependentPackageInfo(const TArray<FString>& PackageNames, int32 NumAsyncDependencyTasks);

	/**
	 * Get all dependent package hashes
	 */
	bool RecursiveGetDependentPackageHashes(const TCHAR* InPackageName, TMap<FString, FMD5Hash>& Dependencies);


	/**
	 * Get the sorted list of generated package dependencies
	 *
	 * @param PackageName name of the package to get dependencies for 
	 * @param Dependencies list of dependencies
	 * @return true if succeeded and package exists false if package doesn't exist
	 */
	bool GetPackageDependencies( const TCHAR* PackageName, TArray<FString>& Dependencies );

protected:
	/** Determine the newest shader source time stamp */
	void DetermineShaderSourceTimeStamp();

	/** Determine the newest 'script' time stamp */
	void DetermineScriptSourceTimeStamp();

	/** Determine the newest engine 'script' time stamp */
	void DetermineEngineScriptSourceTimeStamp();

	/** Determine the newest game 'script' time stamp */
	void DetermineGameScriptSourceTimeStamp();

	/**
	 *	Determine the newest 'script' source file for either the game or the engine
	 *
	 *	@param	bInGame				true if looking at game script, false if engine
	 *	@param	OutNewestTime		OUTPUT - the timestamp of the newest 'script' file
	 */
	void DetermineScriptSourceTimeStamp(bool bInGame, FDateTime& OutNewestTime);


	void DetermineScriptSourceTimeStampForModule(const FString& ModuleName, const FString& ModuleSourcePath);

	/**
	 * Initalize the package dependency info which then can be used to get timestamps 
	 */
	bool InitializeDependencyInfo(const TCHAR* InPackageName, FPackageDependencyTrackingInfo*& OutPkgInfo);

	bool DeterminePackageDependencies( FPackageDependencyTrackingInfo* PkgInfo, TSet<FPackageDependencyTrackingInfo*>& AllTouchedPackages );


	/**
	 * Determine newest script source file in the path given (engine, game, uplugin)
	 *
	 * @param Path		root path of the plugin / engine
	 */
	void DetermineScriptSourceTimeStamp(FString Path);

	/** Prep the content package list - ie gather the list of all content files and their actual timestamps */
	void PrepContentPackageTimeStamps();

	/**
	 * Recursivly resolve the dependent hashes by hashing full file hashes of dependencies
	 * detect circular references
	 * 
	 * @param PkgInfo package we want to generate the dependent hash for
	 * @param OutHash generated dependent hash
	 */
	void RecursiveResolveDependentHash(FPackageDependencyTrackingInfo* PkgInfo, FMD5 &OutHash, FDateTime& EarliestTime);



	/** The newest time stamp of the shader source files. Used when a package contains a material */
	FDateTime ShaderSourceTimeStamp;
	/** The newest shader source file - for informational purposes only */
	FString NewestShaderSource;
	/** The pkg info for shader source */
	FPackageDependencyTrackingInfo* ShaderSourcePkgInfo;

	/** The newest time stamp of the engine 'script' source files. Used when a package contains a blueprint */
	FDateTime EngineScriptSourceTimeStamp;
	/** The newest engine 'script' source file - for informational purposes only */
	FString NewestEngineScript;
	/** The newest time stamp of the game 'script' source files. Used when a package contains a blueprint */
	FDateTime GameScriptSourceTimeStamp;
	/** The newest script source time stamp */
	FDateTime ScriptSourceTimeStamp;
	/** The pkg info for script source */
	FPackageDependencyTrackingInfo* ScriptSourcePkgInfo;

	/** The package information, including dependencies for content files */
	TMap<FString,class FPackageDependencyTrackingInfo*> PackageInformation;

};
