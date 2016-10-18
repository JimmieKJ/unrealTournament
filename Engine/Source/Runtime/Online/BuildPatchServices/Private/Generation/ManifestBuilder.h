// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core/BlockStructure.h"
#include "FileAttributesParser.h"
#include "BuildStreamer.h"

namespace BuildPatchServices
{
	struct FManifestDetails
	{
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

	class IManifestBuilder
	{
	public:
		virtual void AddChunkMatch(const FGuid& ChunkGuid, const FBlockStructure& Structure) = 0;
		virtual bool FinalizeData(const TArray<FFileSpan>& FileSpans, TArray<FChunkInfoData> ChunkInfo) = 0;
		virtual bool SaveToFile(const FString& Filename) = 0;
	};

	typedef TSharedRef<IManifestBuilder> IManifestBuilderRef;
	typedef TSharedPtr<IManifestBuilder> IManifestBuilderPtr;

	class FManifestBuilderFactory
	{
	public:
		static IManifestBuilderRef Create(const FManifestDetails& Details);
	};
}
