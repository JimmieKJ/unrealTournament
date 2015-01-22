// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// WARNING! Copied from EngineAnalytics.h

class IAnalyticsProvider;

/**
 * The public interface for the engine's analytics provider singleton.
 * For Epic builds, this will point to epic's internal analytics provider.
 * For licensee builds, it will be NULL by default unless they provide their own
 * configuration.
 * 
 */
class FCrashReportAnalytics : FNoncopyable
{
public:
	/**
	 * Return the provider instance. Not valid outside of Initialize/Shutdown calls.
	 * Note: must check IsAvailable() first else this code will assert if the provider is not valid.
	 */
	static IAnalyticsProvider& GetProvider();
	/** Helper function to determine if the provider is valid. */
	static bool IsAvailable() 
	{ 
		return Analytics.IsValid(); 
	}
	/** Called to initialize the singleton. */
	static void Initialize();
	/** Called to shut down the singleton */
	static void Shutdown();

private:
	static bool bIsInitialized;
	static TSharedPtr<IAnalyticsProvider> Analytics;
};

