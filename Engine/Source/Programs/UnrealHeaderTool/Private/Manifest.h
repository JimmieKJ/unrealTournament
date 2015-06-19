// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UnrealSourceFile.h"


struct FManifestModule
{
	/** The name of the module */
	FString Name;

	/** Module type */
	EBuildModuleType::Type ModuleType;

	/** Long package name for this module's UObject class */
	FString LongPackageName;

	/** Base directory of this module on disk */
	FString BaseDirectory;

	/** The directory to which #includes from this module should be relative */
	FString IncludeBase;

	/** Directory where generated include files should go */
	FString GeneratedIncludeDirectory;

	/** List of C++ public 'Classes' header files with UObjects in them (legacy) */
	TArray<FString> PublicUObjectClassesHeaders;

	/** List of C++ public header files with UObjects in them */
	TArray<FString> PublicUObjectHeaders;

	/** List of C++ private header files with UObjects in them */
	TArray<FString> PrivateUObjectHeaders;

	/** Absolute path to the module's PCH */
	FString PCH;

	/** Base (i.e. extensionless) path+filename of where to write out the module's .generated.* files */
	FString GeneratedCPPFilenameBase;

	/** Whether or not to write out headers that have changed */
	bool SaveExportedHeaders;

	/** Version of generated code. */
	EGeneratedCodeVersion GeneratedCodeVersion;
};


struct FManifest
{
	bool    IsGameTarget;
	FString RootLocalPath;
	FString RootBuildPath;
	FString TargetName;
	
	/** Ordered list of modules that define UObjects or UStructs, which we may need to generate
	    code for.  The list is in module dependency order, such that most dependent modules appear first. */
	TArray<FManifestModule> Modules;

	/**
	 * Loads an UnrealHeaderTool.manifest from the specified filename.
	 *
	 * @param Filename The filename of the manifest to load.
	 * @return The loaded module info.
	 */
	static FManifest LoadFromFile(const FString& Filename);
};
