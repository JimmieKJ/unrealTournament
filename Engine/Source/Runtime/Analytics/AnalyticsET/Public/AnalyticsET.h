// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAnalyticsProviderModule.h"
#include "Core.h"

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
		 * AppVersion - defines the app version passed to the provider. By default this will be GEngineVersion, but you can supply your own. 
		 * As a convenience, you can use -AnalyticsAppVersion=XXX to force the AppVersion to a specific value. Useful for playtest etc where you want to define a specific version string dynamically.
		 * If you supply your own Version string, occurrences of "%VERSION%" are replaced with GEngineVersion. ie, -AnalyticsAppVersion=MyCustomID-%VERSION%.
		 */
		FString AppVersionET;
		/** UseDataRouter - flag indicating that we should use the new datarouter system. */
		FString UseDataRouterET;
		/** DataRouterUploadURLET - data router upload URL - Defaults if empty to GetDefaultDataRouterUploadURL. */
		FString DataRouterUploadURLET;

		/** KeyName required for APIKey configuration. */
		static FString GetKeyNameForAPIKey() { return TEXT("APIKeyET"); }
		/** KeyName required for APIServer configuration. */
		static FString GetKeyNameForAPIServer() { return TEXT("APIServerET"); }
		/** KeyName required for AppVersion configuration. */
		static FString GetKeyNameForAppVersion() { return TEXT("AppVersionET"); }
		/** Default value if no APIServer configuration is provided. */
		static FString GetDefaultAPIServer() { return TEXT("http://devonline-02:/ETAP/"); }
		/** Default value if no DataRouterServer configuration is provided. */
		static FString GetDefaultDataRouterUploadURL() { return TEXT("https://datarouter-public-service-gamedev.ol.epicgames.net/datarouter/api/v1/public/data"); }
		/** KeyName indicating if we should use the data router. */
		static FString GetKeyNameForUseDataRouter() { return TEXT("UseDataRouterET"); }
		/** KeyName for the data router upload URL. */
		static FString GetKeyNameForDataRouterUploadURL() { return TEXT("DataRouterUploadURLET"); }
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

