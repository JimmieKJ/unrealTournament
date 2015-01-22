// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleDescriptor.h"
#include "PluginDescriptor.h"

/**
 * Version numbers for project descriptors.
 */ 
namespace EProjectDescriptorVersion
{
	enum Type
	{
		Invalid = 0,
		Initial = 1,
		NameHash = 2,
		ProjectPluginUnification = 3,
		// !!!!!!!!!! IMPORTANT: Remember to also update LatestPluginDescriptorFileVersion in Plugins.cs (and Plugin system documentation) when this changes!!!!!!!!!!!
		// -----<new versions can be added before this line>-------------------------------------------------
		// - this needs to be the last line (see note below)
		LatestPlusOne,
		Latest = LatestPlusOne - 1
	};
}

/**
 * Descriptor for projects. Contains all the information contained within a .uproject file.
 */
struct PROJECTS_API FProjectDescriptor
{
	/** Descriptor version number. */
	EProjectDescriptorVersion::Type FileVersion;

	/** The associated engine label. Used by UnrealVersionSelector to open projects with the correct editor */
	FString EngineAssociation;

	/** Category to show under the project browser */
	FString Category;

	/** Description to show in the project browser */
	FString Description;

	/** List of all modules associated with this project */
	TArray<FModuleDescriptor> Modules;

	/** List of plugins for this project (may be enabled/disabled) */
	TArray<FPluginReferenceDescriptor> Plugins;

	/** Array of platforms that this project is targeting */
	TArray<FName> TargetPlatforms;

	/** A hash that is used to determine if the project was forked from a sample */
	uint32 EpicSampleNameHash;

	/** Constructor. */
	FProjectDescriptor();

	/** Signs the project given for the given filename */
	void Sign(const FString& FilePath);

	/** Checks whether the descriptor is signed */
	bool IsSigned(const FString& FilePath) const;

	/** Updates the supported target platforms list */
	void UpdateSupportedTargetPlatforms(const FName& InPlatformName, bool bIsSupported);

	/** Loads the descriptor from the given file. */
	bool Load(const FString& FileName, FText& OutFailReason);

	/** Reads the descriptor from the given JSON object */
	bool Read(const FJsonObject& Object, FText& OutFailReason);

	/** Saves the descriptor to the given file. */
	bool Save(const FString& FileName, FText& OutFailReason);

	/** Writes the descriptor to the given JSON object */
	void Write(TJsonWriter<>& Writer) const;

	/** Returns the extension used for project descriptors (uproject) */
	static FString GetExtension();
};
