// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Stores a record of a built target, with all metadata that other tools may need to know about the build.
 */
class PROJECTS_API FVersionManifest
{
public:
	uint32 Changelist;
	uint32 CompatibleChangelist;
	FString BuildId;
	TMap<FString, FString> ModuleNameToFileName;

	/**
	 * Default constructor 
	 */
	FVersionManifest();

	/**
	 * Gets the path to a version manifest for the given folder.
	 *
	 * @param DirectoryName		Directory to read from
	 * @param bIsGameFolder		Whether the directory is a game folder of not. Used to adjust the name if the application is running in DebugGame.
	 * @return The filename of the version manifest.
	 */
	static FString GetFileName(const FString& DirectoryName, bool bIsGameFolder);

	/**
	 * Read a version manifest from disk.
	 *
	 * @param FileName		Filename to read from
	 */
	static bool TryRead(const FString& FileName, FVersionManifest& Manifest);
};

/**
 * Adapter for the module manager to be able to read and Enumerates the contents Stores a record of a built target, with all metadata that other tools may need to know about the build.
 */
class PROJECTS_API FVersionedModuleEnumerator
{
public:
	FVersionedModuleEnumerator();
	bool RegisterWithModuleManager();
	const FVersionManifest& GetInitialManifest() const;

private:
	FVersionManifest InitialManifest;

	void QueryModules(const FString& InDirectoryName, bool bIsGameDirectory, TMap<FString, FString>& OutModules) const;
	bool IsMatchingVersion(const FVersionManifest& Manifest) const;
};
