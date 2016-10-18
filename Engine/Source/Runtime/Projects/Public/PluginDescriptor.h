// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleDescriptor.h"
#include "LocalizationDescriptor.h"
#include "CustomBuildSteps.h"

/**
 * Version numbers for plugin descriptors.
 */ 
namespace EPluginDescriptorVersion
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
 * Descriptor for plugins. Contains all the information contained within a .uplugin file.
 */
struct PROJECTS_API FPluginDescriptor
{
	/** Descriptor version number */
	int32 FileVersion;

	/** Version number for the plugin.  The version number must increase with every version of the plugin, so that the system 
	    can determine whether one version of a plugin is newer than another, or to enforce other requirements.  This version
		number is not displayed in front-facing UI.  Use the VersionName for that. */
	int32 Version;

	/** Name of the version for this plugin.  This is the front-facing part of the version number.  It doesn't need to match
	    the version number numerically, but should be updated when the version number is increased accordingly. */
	FString VersionName;

	/** Friendly name of the plugin */
	FString FriendlyName;

	/** Description of the plugin */
	FString Description;

	/** The name of the category this plugin */
	FString Category;

	/** The company or individual who created this plugin.  This is an optional field that may be displayed in the user interface. */
	FString CreatedBy;

	/** Hyperlink URL string for the company or individual who created this plugin.  This is optional. */
	FString CreatedByURL;

	/** Documentation URL string. */
	FString DocsURL;

	/** Marketplace URL for this plugin. This URL will be embedded into projects that enable this plugin, so we can redirect to the marketplace if a user doesn't have it installed. */
	FString MarketplaceURL;

	/** Support URL/email for this plugin. */
	FString SupportURL;

	/** List of all modules associated with this plugin */
	TArray<FModuleDescriptor> Modules;

	/** List of all localization targets associated with this plugin */
	TArray<FLocalizationTargetDescriptor> LocalizationTargets;

	/** Whether this plugin should be enabled by default for all projects */
	bool bEnabledByDefault;

	/** Can this plugin contain content? */
	bool bCanContainContent;

	/** Marks the plugin as beta in the UI */
	bool bIsBetaVersion;

	/** Signifies that the plugin was installed on top of the engine */
	bool bInstalled;

	/** For plugins that are under a platform folder (eg. /PS4/), determines whether compiling the plugin requires the build platform and/or SDK to be available */
	bool bRequiresBuildPlatform;

	/** Pre-build steps for each host platform */
	FCustomBuildSteps PreBuildSteps;

	/** Pre-build steps for each host platform */
	FCustomBuildSteps PostBuildSteps;

	/** Constructor. */
	FPluginDescriptor();

	/** Loads the descriptor from the given file. */
	bool Load(const FString& FileName, bool bPluginTypeEnabledByDefault, FText& OutFailReason);

	/** Reads the descriptor from the given JSON object */
	bool Read(const FString& Text, bool bPluginTypeEnabledByDefault, FText& OutFailReason);

	/** Saves the descriptor from the given file. */
	bool Save(const FString& FileName, bool bPluginTypeEnabledByDefault, FText& OutFailReason) const;

	/** Writes a descriptor to JSON */
	void Write(FString& Text, bool bPluginTypeEnabledByDefault) const;
};

/**
 * Descriptor for a plugin reference. Contains the information required to enable or disable a plugin for a given platform.
 */
struct PROJECTS_API FPluginReferenceDescriptor
{
	/** Name of the plugin */
	FString Name;

	/** Whether it should be enabled by default */
	bool bEnabled;

	/** Whether this plugin is optional, and the game should silently ignore it not being present */
	bool bOptional;
	
	/** Description of the plugin for users that do not have it installed. */
	FString Description;

	/** URL for this plugin on the marketplace, if the user doesn't have it installed. */
	FString MarketplaceURL;

	/** If enabled, list of platforms for which the plugin should be enabled (or all platforms if blank). */
	TArray<FString> WhitelistPlatforms;

	/** If enabled, list of platforms for which the plugin should be disabled. */
	TArray<FString> BlacklistPlatforms;
 
	/** If enabled, list of targets for which the plugin should be enabled (or all targets if blank). */
	TArray<FString> WhitelistTargets;

	/** If enabled, list of targets for which the plugin should be disabled. */
	TArray<FString> BlacklistTargets;

	/** Constructor */
	FPluginReferenceDescriptor(const FString& InName = TEXT(""), bool bInEnabled = false, const FString& InMarketplaceURL = TEXT(""));

	/** Determines whether the plugin is enabled for the given platform */
	bool IsEnabledForPlatform(const FString& Platform) const;

	/** Determines whether the plugin is enabled for the given target */
	bool IsEnabledForTarget(const FString& Target) const;

	/** Reads the descriptor from the given JSON object */
	bool Read(const FJsonObject& Object, FText& OutFailReason);

	/** Reads an array of modules from the given JSON object */
	static bool ReadArray(const FJsonObject& Object, const TCHAR* Name, TArray<FPluginReferenceDescriptor>& OutModules, FText& OutFailReason);

	/** Writes a descriptor to JSON */
	void Write(TJsonWriter<>& Writer) const;

	/** Writes an array of modules to JSON */
	static void WriteArray(TJsonWriter<>& Writer, const TCHAR* Name, const TArray<FPluginReferenceDescriptor>& Modules);
};

