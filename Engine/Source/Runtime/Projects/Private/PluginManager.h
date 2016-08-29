// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PluginDescriptor.h"

/**
 * Instance of a plugin in memory
 */
class FPlugin : public IPlugin
{
public:
	/** The name of the plugin */
	FString Name;

	/** The filename that the plugin was loaded from */
	FString FileName;

	/** The plugin's settings */
	FPluginDescriptor Descriptor;

	/** Where does this plugin live? */
	EPluginLoadedFrom LoadedFrom;

	/** True if the plugin is marked as enabled */
	bool bEnabled;

	/**
	 * FPlugin constructor
	 */
	FPlugin(const FString &FileName, const FPluginDescriptor& InDescriptor, EPluginLoadedFrom InLoadedFrom);

	/**
	 * Destructor.
	 */
	virtual ~FPlugin();

	/* IPluginInfo interface */
	virtual FString GetName() const override;
	virtual FString GetDescriptorFileName() const override;
	virtual FString GetBaseDir() const override;
	virtual FString GetContentDir() const override;
	virtual FString GetMountedAssetPath() const override;
	virtual bool IsEnabled() const override;
	virtual bool CanContainContent() const override;
	virtual EPluginLoadedFrom GetLoadedFrom() const override;
	virtual const FPluginDescriptor& GetDescriptor() const override;
	virtual bool UpdateDescriptor(const FPluginDescriptor& NewDescriptor, FText& OutFailReason) override;
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
	virtual void RefreshPluginsList() override;
	virtual bool LoadModulesForEnabledPlugins( const ELoadingPhase::Type LoadingPhase ) override;
	virtual void GetLocalizationPathsForEnabledPlugins( TArray<FString>& OutLocResPaths ) override;
	virtual void SetRegisterMountPointDelegate( const FRegisterMountPointDelegate& Delegate ) override;
	virtual bool AreRequiredPluginsAvailable() override;
	virtual bool CheckModuleCompatibility( TArray<FString>& OutIncompatibleModules ) override;
	virtual TSharedPtr<IPlugin> FindPlugin(const FString& Name) override;
	virtual TArray<TSharedRef<IPlugin>> GetEnabledPlugins() override;
	virtual TArray<TSharedRef<IPlugin>> GetDiscoveredPlugins() override;
	virtual TArray< FPluginStatus > QueryStatusForAllPlugins() const override;
	virtual void AddPluginSearchPath(const FString& ExtraDiscoveryPath, bool bRefresh = true) override;

private:

	/** Searches for all plugins on disk and builds up the array of plugin objects.  Doesn't load any plugins. 
	    This is called when the plugin manager singleton is first accessed. */
	void DiscoverAllPlugins();

	/** Reads all the plugin descriptors */
	static void ReadAllPlugins(TMap<FString, TSharedRef<FPlugin>>& Plugins, const TSet<FString>& ExtraSearchPaths);

	/** Reads all the plugin descriptors from disk */
	static void ReadPluginsInDirectory(const FString& PluginsDirectory, const EPluginLoadedFrom LoadedFrom, TMap<FString, TSharedRef<FPlugin>>& Plugins);

	/** Finds all the plugin descriptors underneath a given directory */
	static void FindPluginsInDirectory(const FString& PluginsDirectory, TArray<FString>& FileNames);

	/** Sets the bPluginEnabled flag on all plugins found from DiscoverAllPlugins that are enabled in config */
	bool ConfigureEnabledPlugins();

	/** Gets the instance of a given plugin */
	TSharedPtr<FPlugin> FindPluginInstance(const FString& Name);

	/** Returns true if the plugin is supported by the current target (program/game) */
	bool IsPluginSupportedByCurrentTarget(TSharedRef<FPlugin> Plugin) const;

private:
	/** All of the plugins that we know about */
	TMap< FString, TSharedRef< FPlugin > > AllPlugins;

	/** Delegate for mounting content paths.  Bound by FPackageName code in CoreUObject, so that we can access
	    content path mounting functionality from Core. */
	FRegisterMountPointDelegate RegisterMountPointDelegate;

	/** Set when all the appropriate plugins have been marked as enabled */
	bool bHaveConfiguredEnabledPlugins;

	/** Set if all the required plugins are available */
	bool bHaveAllRequiredPlugins;

	/** List of additional directory paths to search for plugins within */
	TSet<FString> PluginDiscoveryPaths;
};


