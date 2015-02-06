// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IAnalyticsProvider;

/**
 * The public interface for the engine's analytics provider singleton.
 * For Epic builds, this will point to epic's internal analytics provider.
 * For licensee builds, it will be NULL by default unless they provide their own
 * configuration.
 * 
 */
class FEngineAnalytics : FNoncopyable
{
public:
	/**
	 * Return the provider instance. Not valid outside of Initialize/Shutdown calls.
	 * Note: must check IsAvailable() first else this code will assert if the provider is not valid.
	 */
	static ENGINE_API IAnalyticsProvider& GetProvider();
	/** Helper function to determine if the provider is valid. */
	static ENGINE_API bool IsAvailable() { return bShouldSendUsageEvents && Analytics.IsValid(); }
	/** Called to initialize the singleton. */
	static void Initialize();
	/** Called to shut down the singleton */
	static void Shutdown();

private:
	static bool bIsInitialized;
	/** This allows us to "opt-out" of engine analytics. We still create a session, but we don't send any usage events. */
	static bool bShouldSendUsageEvents;
	static TSharedPtr<IAnalyticsProvider> Analytics;
};

