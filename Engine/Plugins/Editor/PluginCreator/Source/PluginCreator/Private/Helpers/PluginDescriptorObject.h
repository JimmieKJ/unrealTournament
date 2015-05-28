// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "PluginDescriptor.h"
#include "PluginDescriptorObject.generated.h"

/**
*	We use this object to display plugin properties using details view.
*/
UCLASS()
class UPluginDescriptorObject : public UObject
{
	GENERATED_BODY()

	UPluginDescriptorObject(const FObjectInitializer& ObjectInitializer);

public:
	// Should the project files be regenerated when the plugin is created?
	UPROPERTY(EditAnywhere, Category = "CreationOptions")
	bool bRegenerateProjectFiles;

	// Should the plugin folder be shown in the file system when the plugin is created?
	UPROPERTY(EditAnywhere, Category = "CreationOptions")
	bool bShowPluginFolder;

	/** Descriptor version number */
	UPROPERTY(VisibleAnywhere, Category = "PluginDescription")
	int32 FileVersion;

	/** Version number for the plugin.  The version number must increase with every version of the plugin, so that the system
	can determine whether one version of a plugin is newer than another, or to enforce other requirements.  This version
	number is not displayed in front-facing UI.  Use the VersionName for that. */
	UPROPERTY(VisibleAnywhere, Category = "PluginDescription")
	int32 Version;

	/** Name of the version for this plugin.  This is the front-facing part of the version number.  It doesn't need to match
	the version number numerically, but should be updated when the version number is increased accordingly. */
	UPROPERTY(EditAnywhere, Category = "PluginDescription")
	FString VersionName;

	/** Friendly name of the plugin */
	UPROPERTY(EditAnywhere, Category = "PluginDescription")
	FString FriendlyName;

	/** Description of the plugin */
	UPROPERTY(EditAnywhere, Category = "PluginDescription")
	FString Description;

	/** The category that this plugin belongs to */
	UPROPERTY(EditAnywhere, Category = "PluginDescription")
	FString Category;

	/** The company or individual who created this plugin.  This is an optional field that may be displayed in the user interface. */
	UPROPERTY(EditAnywhere, Category = "PluginDescription")
	FString CreatedBy;

	/** Hyperlink URL string for the company or individual who created this plugin.  This is optional. */
	UPROPERTY(EditAnywhere, Category = "PluginDescription")
	FString CreatedByURL;

	/** Documentation URL string. */
	UPROPERTY(EditAnywhere, Category = "PluginDescription")
	FString DocsURL;

	/** List of all modules associated with this plugin */
	TArray<FModuleDescriptor> Modules;

	/** Can this plugin contain content? */
	UPROPERTY(EditAnywhere, Category = "PluginDescription")
	bool bCanContainContent;

	/** Marks the plugin as beta in the UI */
	UPROPERTY(EditAnywhere, Category = "PluginDescription")
	bool bIsBetaVersion;

public:
	void FillDescriptor(FPluginDescriptor& OutDescriptor);

};
