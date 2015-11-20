// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "IPluginBrowser.h"
#include "IPluginManager.h"
#include "ModuleManager.h"

DECLARE_MULTICAST_DELEGATE(FOnNewPluginCreated);

class FPluginBrowserModule : public IPluginBrowser
{
public:
	/** Accessor for the module interface */
	static FPluginBrowserModule& Get()
	{
		return FModuleManager::Get().GetModuleChecked<FPluginBrowserModule>(TEXT("PluginBrowser"));
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Gets a delegate so that you can register/unregister to receive callbacks when plugins are created */
	FOnNewPluginCreated& OnNewPluginCreated() {return NewPluginCreatedDelegate;}

	/** Broadcasts callback to notify registrants that a plugin has been created */
	void BroadcastNewPluginCreated() const {NewPluginCreatedDelegate.Broadcast();}

	/**
	 * Sets whether a plugin is pending enable/disable
	 * @param PluginName The name of the plugin
	 * @param bCurrentlyEnabled The current state of this plugin, so that we can decide whether a change is no longer pending
	 * @param bPendingEnabled Whether to set this plugin to pending enable or disable
	 */
	void SetPluginPendingEnableState(const FString& PluginName, bool bCurrentlyEnabled, bool bPendingEnabled);

	/**
	 * Gets whether a plugin is pending enable/disable
	 * This should only be used when you know this is the case after using HasPluginPendingEnable
	 * @param PluginName The name of the plugin
	 */
	bool GetPluginPendingEnableState(const FString& PluginName) const;

	/** Whether there are any plugins pending enable/disable */
	bool HasPluginsPendingEnable() const {return PendingEnablePlugins.Num() > 0;}

	/**
	 * Whether a specific plugin is pending enable/disable
	 * @param PluginName The name of the plugin
	 */
	bool HasPluginPendingEnable(const FString& PluginName) const;

	/** ID name for the plugins editor major tab */
	static const FName PluginsEditorTabName;

	/** ID name for the plugin creator tab */
	static const FName PluginCreatorTabName;

private:
	/** Called to spawn the plugin creator tab */
	TSharedRef<SDockTab> HandleSpawnPluginCreatorTab(const FSpawnTabArgs& SpawnTabArgs);

	/** List of plugins that are pending enable/disable */
	TMap<FString, bool> PendingEnablePlugins;

	/** Delegate called when a new plugin is created */
	FOnNewPluginCreated NewPluginCreatedDelegate;
};
