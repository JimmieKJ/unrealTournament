// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAnalyticsProviderModule.h"
#include "Core.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAnalytics, Display, All);

class IAnalyticsProvider;

/**
 * The public interface to this module
 */
class FAnalyticsET : public IAnalyticsProviderModule
{
	//--------------------------------------------------------------------------
	// Module functionality
	//--------------------------------------------------------------------------
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FAnalyticsET& Get()
	{
		return FModuleManager::LoadModuleChecked< FAnalyticsET >( "AnalyticsET" );
	}

	//--------------------------------------------------------------------------
	// Configuration functionality
	//--------------------------------------------------------------------------
public:
	/** Defines required configuration values for ET analytics provider. */
	struct Config
	{
		/** ET APIKey - Get from your account manager */
		FString APIKeyET;
		/** ET API Server - Defaults if empty to GetDefaultAPIServer. */
		FString APIServerET;
		/** 
		 * AppVersion - defines the app version passed to the provider. By default this will be FEngineVersion::Current(), but you can supply your own. 
		 * As a convenience, you can use -AnalyticsAppVersion=XXX to force the AppVersion to a specific value. Useful for playtest etc where you want to define a specific version string dynamically.
		 * If you supply your own Version string, occurrences of "%VERSION%" are replaced with FEngineVersion::Current(). ie, -AnalyticsAppVersion=MyCustomID-%VERSION%.
		 */
		FString AppVersionET;
		/** When true, sends events using the legacy ET protocol that passes all attributes as URL parameters. Defaults to false. */
		bool UseLegacyProtocol;

		/** Default ctor to ensure all values have their proper default. */
		Config() : UseLegacyProtocol(false) {}

		/** KeyName required for APIKey configuration. */
		static FString GetKeyNameForAPIKey() { return TEXT("APIKeyET"); }
		/** KeyName required for APIServer configuration. */
		static FString GetKeyNameForAPIServer() { return TEXT("APIServerET"); }
		/** KeyName required for AppVersion configuration. */
		static FString GetKeyNameForAppVersion() { return TEXT("AppVersionET"); }
		/** Optional parameter to use the legacy backend protocol. */
		static FString GetKeyNameForUseLegacyProtocol() { return TEXT("UseLegacyProtocol"); }
		/** Default value if no APIServer configuration is provided. */
		static FString GetDefaultAPIServer() { return TEXT("http://devonline-02:/ETAP/"); }
	};

	//--------------------------------------------------------------------------
	// provider factory functions
	//--------------------------------------------------------------------------
public:
	/**
	 * IAnalyticsProviderModule interface.
	 * Creates the analytics provider given a configuration delegate.
	 * The keys required exactly match the field names in the Config object. 
	 */
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const FAnalytics::FProviderConfigurationDelegate& GetConfigValue) const override;
	
	/** 
	 * Construct an analytics provider directly from a config object.
	 */
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const Config& ConfigValues) const;

private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

