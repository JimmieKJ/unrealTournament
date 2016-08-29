// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

/**
* A helper for passing patch settings around
*/
struct FBuildPatchSettings
{
	// The directory to analyze
	FString RootDirectory;
	// The ID of the app of this build
	uint32 AppId;
	// The name of the app of this build
	FString AppName;
	// The version string for this build
	FString BuildVersion;
	// The local exe path that would launch this build
	FString LaunchExe;
	// The command line that would launch this build
	FString LaunchCommand;
	// The path to a file containing a \r\n separated list of RootDirectory relative files to ignore.
	FString IgnoreListFile;
	// The path to a file containing a \r\n separated list of RootDirectory relative files to followed by attribute keywords.
	FString AttributeListFile;
	// The display name of the prerequisites installer
	FString PrereqName;
	// The path to the prerequisites installer
	FString PrereqPath;
	// The command line arguments for the prerequisites installer
	FString PrereqArgs;
	// The maximum age (in days) of existing data files which can be reused in this build
	float DataAgeThreshold;
	// Indicates whether data age threshold should be honored. If false, ALL data files can be reused
	bool bShouldHonorReuseThreshold;
	// Map of custom fields to add to the manifest
	TMap<FString, FVariant> CustomFields;
	// The output manifest filename.
	FString OutputFilename;
};
