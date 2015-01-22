// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PluginDescriptor.h"

/**
 * Enum for where a plugin is loaded from
 */
struct EPluginLoadedFrom
{
	enum Type
	{
		/** Plugin is built-in to the engine */
		Engine,

		/** Project-specific plugin, stored within a game project directory */
		GameProject
	};
};

/**
 * Instance of a plugin in memory
 */
class FPluginInstance
{
public:
	/** The name of the plugin */
	FString Name;

	/** The filename that the plugin was loaded from */
	FString FileName;

	/** The plugin's settings */
	FPluginDescriptor Descriptor;

	/** Where does this plugin live? */
	EPluginLoadedFrom::Type LoadedFrom;

	/** True if the plugin is marked as enabled */
	bool bEnabled;

	/**
	 * FPlugin constructor
	 */
	FPluginInstance(const FString &FileName, const FPluginDescriptor& InDescriptor, EPluginLoadedFrom::Type InLoadedFrom);
};

/**
 * FPluginManager manages available code and content extensions (both loaded and not loaded.)
 */
class FPluginManager : public IPluginManager
{
public:
	/** Constructor */
	FPluginManager();

	/** Destructor */
	~FPluginManager();

	/** IPluginManager interface */
	virtual bool LoadModulesForEnabledPlugins( const ELoadingPhase::Type LoadingPhase ) override;
	virtual void SetRegisterMountPointDelegate( const FRegisterMountPointDelegate& Delegate ) override;
	virtual bool AreRequiredPluginsAvailable() override;
	virtual bool CheckModuleCompatibility( TArray<FString>& OutIncompatibleModules ) override;
	virtual TArray< FPluginStatus > QueryStatusForAllPlugins() const override;
	virtual const TArray< FPluginContentFolder >& GetPluginContentFolders() const override;

private:

	/** Searches for all plugins on disk and builds up the array of plugin objects.  Doesn't load any plugins. 
	    This is called when the plugin manager singleton is first accessed. */
	void DiscoverAllPlugins();

	/** Sets the bPluginEnabled flag on all plugins found from DiscoverAllPlugins that are enabled in config */
	bool ConfigureEnabledPlugins();

	/** Gets the instance of a given plugin */
	TSharedPtr<FPluginInstance> FindPluginInstance(const FString& Name);

private:
	/** All of the plugins that we know about */
	TArray< TSharedRef< FPluginInstance > > AllPlugins;

	/** All the plugin content folders */
	TArray<FPluginContentFolder> ContentFolders;

	/** Delegate for mounting content paths.  Bound by FPackageName code in CoreUObject, so that we can access
	    content path mounting functionality from Core. */
	FRegisterMountPointDelegate RegisterMountPointDelegate;

	/** Set when all the appropriate plugins have been marked as enabled */
	bool bHaveConfiguredEnabledPlugins;

	/** Set if all the required plugins are available */
	bool bHaveAllRequiredPlugins;
};


