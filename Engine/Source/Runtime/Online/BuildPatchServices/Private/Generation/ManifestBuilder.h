// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_BUILDPATCHGENERATION

#include "DataScanner.h"
#include "BuildStreamer.h"

namespace BuildPatchServices
{
	struct FManifestDetails
	{
		// Whether this manifest is describing nochunks build
		bool bIsFileData;
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
		// The display name of the prerequisites installer
		FString PrereqName;
		// The path to the prerequisites installer
		FString PrereqPath;
		// The command line arguments for the prerequisites installer
		FString PrereqArgs;
		// Map of custom fields to add to the manifest
		TMap<FString, FVariant> CustomFields;
		// Map of file attributes
		TMap<FString, FFileAttributes> FileAttributesMap;
	};

	class FManifestBuilder
	{
	public:
		virtual void AddDataScanner(FDataScannerRef Scanner) = 0;
		virtual void SaveToFile(const FString& Filename) = 0;
	};

	typedef TSharedRef<FManifestBuilder, ESPMode::ThreadSafe> FManifestBuilderRef;
	typedef TSharedPtr<FManifestBuilder, ESPMode::ThreadSafe> FManifestBuilderPtr;

	class FManifestBuilderFactory
	{
	public:
		static FManifestBuilderRef Create(const FManifestDetails& Details, const FBuildStreamerRef& BuildStreamer);
	};
}

#endif
