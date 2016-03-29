// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class IAnalyticsProvider;
class FEngineSessionManager;

/**
 * The public interface for the engine's analytics provider singleton.
 * For Epic builds, this will point to epic's internal analytics provider.
 * For licensee builds, it will be NULL by default unless they provide their own
 * configuration.
 * 
 * WARNING: This is an analytics provider instance that is created whenever UE4 is launched. 
 * It is intended ONLY for use by Epic Games. This is NOT intended for games to send 
 * game-specific telemetry. Create your own provider instance for your game and configure
 * it independently.
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
	static ENGINE_API bool IsAvailable() { return Analytics.IsValid(); }
	/** Called to initialize the singleton. */
	static ENGINE_API void Initialize();
	/** Called to shut down the singleton */
	static ENGINE_API void Shutdown(bool bIsEngineShutdown = false);

	static ENGINE_API void Tick(float DeltaTime);

private:
	static bool bIsInitialized;
	static bool bIsEditorRun;
	static bool bIsGameRun;
	static TSharedPtr<IAnalyticsProvider> Analytics;
	static TSharedPtr<FEngineSessionManager> SessionManager;
};

