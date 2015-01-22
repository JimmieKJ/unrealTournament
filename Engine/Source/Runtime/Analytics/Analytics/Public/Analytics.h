// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleManager.h"
#include "AnalyticsEventAttribute.h"

class IAnalyticsProvider;

/**
 * The public interface for interacting with analytics.
 * The basic usage is to call CreateAnalyticsProvider and supply a configuration delegate.
 * Specific analytics providers may choose to provide strongly-typed factory methods for configuration,
 * in which case you are free to call that directly if you know exactly what provider you will be using.
 * This class merely facilitates loosely bound provider configuration so the provider itself can be configured purely via config.
 * 
 * BuildType methods exist as a common way for an analytics provider to configure itself for debug/development/playtest/release scenarios.
 * Again, you can choose to ignore this info and provide a generic configuration delegate that does anything it wants.
 * 
 * To create an analytics provider using all the system defaults, simply call the static GetDefaultConfiguredProvider().
 * 
 */
class FAnalytics : public IModuleInterface
{
	//--------------------------------------------------------------------------
	// Module functionality
	//--------------------------------------------------------------------------
public:
	FAnalytics();
	virtual ~FAnalytics();

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FAnalytics& Get()
	{
		return FModuleManager::LoadModuleChecked< FAnalytics >( "Analytics" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "Analytics" );
	}

	//--------------------------------------------------------------------------
	// Provider Factory functions.
	//--------------------------------------------------------------------------
public:
	/**
	 * Analytics providers must be configured when created.
	 * This delegate provides a generic way to supply this config when creating a provider from this abstract factory function.
	 * This delegate will be called by the specific analytics provider for each configuration value it requires.
	 * The first param is the confguration name, the second parameter is a bool that is true if the value is required.
	 * It is up to the specific provider to define what configuration keys it requires.
	 * Some providers may provide more typesafe ways of constructing them when the code knows exactly which provider it will be using.
	 */
	DECLARE_DELEGATE_RetVal_TwoParams(FString, FProviderConfigurationDelegate, const FString&, bool);

	/**
	 * Factory function to create a specific analytics provider by providing the string name of the provider module, which will be dynamically loaded.
	 * @param ProviderModuleName	The name of the module that contains the specific provider. It must be the primary module interface.
	 * @param GetConfigvalue	Delegate used to configure the provider. The provider will call this delegate once for each key it requires for configuration.
	 * @returns		the analytics provider instance that was created. Could be NULL if initialization failed.
	 */
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const FName& ProviderModuleName, const FProviderConfigurationDelegate& GetConfigValue);

	/**
	 * Creates an instance of the default configured analytics provider.
	 * Default is determined by GetDefaultProviderModuleName and a default constructed ConfigFromIni instance.
	 * 
	 * This implementation is kept in the header file to illustrate how providers are created using configuration delegates.
	 */
	virtual TSharedPtr<IAnalyticsProvider> GetDefaultConfiguredProvider()
	{
		FAnalytics::ConfigFromIni AnalyticsConfig;                     // configure using the default INI sections.
		return FAnalytics::Get().CreateAnalyticsProvider(              // call the factory function
			FAnalytics::ConfigFromIni::GetDefaultProviderModuleName(), // use the default config to find the provider name
			FProviderConfigurationDelegate::CreateRaw(                 // bind to the delegate using default INI sections
				&AnalyticsConfig,                                      // use default INI config sections
				&FAnalytics::ConfigFromIni::GetValue));                
	}

	//--------------------------------------------------------------------------
	// Analytics Build Type determination.
	//--------------------------------------------------------------------------
public:
	/** Defines the different build types from an analytics perspective. Used to determine how to configure the provider. */
	enum BuildType
	{
		/** 
		 * Analytics go into a "slush" account that isn't meant to be representative. 
		 * This is the default mode.
		 */
		Development,
		/**
		 * Test mode for playtests and other runs where the data collected will be semi-representative of actual gameplay. Should be routed to a test, or "representative data" account. 
		 * Use -TESTANALYTICS command line to trigger this mode.
		 */
		Test,
		/** 
		 * Debug mode where analytics should go to the Swrve debug console. Used for feature development and QA testing, since the events are visible on the debug console immediately. 
		 * Use -DEBUGANALYTICS command line to trigger this mode (overrides -TESTANALYTICS).
		 */
		Debug,
		/**
		 * BuildType that should be used by the shipping game. UE_BUILD_SHIPPING builds use this mode (or can use -RELEASEANALYTICS cmdline to force it).
		 */
		Release,
	};

	/** Get the analytics build type. Generally used to determine the keys to use to configure an analytics provider. */
	virtual BuildType GetBuildType() const;

	//--------------------------------------------------------------------------
	// Configuration helper classes and methods
	//--------------------------------------------------------------------------
public:
	/** 
	 * A common way of configuring is from Inis, so this class supports that notion directly by providing a
	 * configuration class with a method suitable to be used as an FProviderConfigurationDelegate
	 * that reads values from the specified ini and section (based on the BuildType).
	 * Also provides a default location to store a provider name, accessed via GetDefaultProviderModuleName().
	 */
	struct ConfigFromIni
	{
		/** 
		 * Create a config using the default values:
		 * IniName - GEngineIni
		 * SectionName (Development) = AnalyticsDevelopment
		 * SectionName (Debug) = AnalyticsDebug
		 * SectionName (Test) = AnalyticsTest
		 * SectionName (Release) = Analytics
		 */
		ConfigFromIni()
			:IniName(GEngineIni)
		{
			SetSectionNameByBuildType(FAnalytics::Get().GetBuildType());
		}

		/** Create a config AS IF the BuildType matches the one passed in. */
		explicit ConfigFromIni(FAnalytics::BuildType InBuildType)
			:IniName(GEngineIni)
		{
			SetSectionNameByBuildType(InBuildType);
		}

		/** Create a config, specifying the Ini name and a single section name for all build types. */
		ConfigFromIni(const FString& InIniName, const FString& InSectionName)
			:IniName(InIniName)
			,SectionName(InSectionName) 
		{
		}

		/** Create a config, specifying the Ini name and the section name for each build type. */
		ConfigFromIni(const FString& InIniName, const FString& SectionNameDevelopment, const FString& SectionNameDebug, const FString& SectionNameTest, const FString& SectionNameRelease)
			:IniName(InIniName)
		{
			FAnalytics::BuildType BuildType = FAnalytics::Get().GetBuildType();
			SectionName = BuildType == FAnalytics::Release 
				? SectionNameRelease
				: BuildType == FAnalytics::Debug
					? SectionNameDebug
					: BuildType == FAnalytics::Test
						? SectionNameTest
						: SectionNameDevelopment;
		}

		/** Method that can be bound to an FProviderConfigurationDelegate. */
		FString GetValue(const FString& KeyName, bool bIsRequired) const
		{
			return FAnalytics::Get().GetConfigValueFromIni(IniName, SectionName, KeyName, bIsRequired);
		}

		/** 
		 * Reads the ProviderModuleName key from the Analytics section of GEngineIni,
		 * which is the default, preferred location to look for the analytics provider name.
		 * This is purely optional, and you can store that information anywhere you want
		 * or merely hardcode the provider module.
		 */
		static FName GetDefaultProviderModuleName()
		{
			FString ProviderModuleName;
			GConfig->GetString(TEXT("Analytics"), TEXT("ProviderModuleName"), ProviderModuleName, GEngineIni);
			return FName(*ProviderModuleName);
		}

		/** Allows setting the INI section name based on the build type passed in. Allows access to the default section values when the application chooses the build type itself. */
		void SetSectionNameByBuildType(FAnalytics::BuildType InBuildType)
		{
			SectionName = InBuildType == FAnalytics::Release 
				? TEXT("Analytics") 
				: InBuildType == FAnalytics::Debug
					? TEXT("AnalyticsDebug") 
					: InBuildType == FAnalytics::Test
						? TEXT("AnalyticsTest") 
						: TEXT("AnalyticsDevelopment");
		}

		/** Ini file name to find the config values. */
		FString IniName;
		/** Section name in the Ini file in which to find the keys. The KeyNames should match the field name in the Config object. */
		FString SectionName;
	};

	/**
	 * Helper for reading configuration values from an INI file (which will be a common scenario). 
	 * This is exposed here so we're not exporting more classes from the module. It's merely a helper for ConfigFromIni struct above.
	 */
	virtual FString GetConfigValueFromIni(const FString& IniName, const FString& SectionName, const FString& KeyName, bool bIsRequired);

private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

