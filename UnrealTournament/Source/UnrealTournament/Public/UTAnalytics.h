// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealTemplate.h"

class IAnalyticsProvider;

class FUTAnalytics : FNoncopyable
{
public:
	static UNREALTOURNAMENT_API IAnalyticsProvider& GetProvider();

	static UNREALTOURNAMENT_API TSharedPtr<IAnalyticsProvider> GetProviderPtr();
	
	/** Helper function to determine if the provider is valid. */
	static UNREALTOURNAMENT_API bool IsAvailable() 
	{ 
		return Analytics.IsValid(); 
	}
	
	/** Called to initialize the singleton. */
	static void Initialize();
	
	/** Called to shut down the singleton */
	static void Shutdown();

	/** 
	 * Called when the login status has changed. Checks IsAvailable() internally, so external code doesn't need to.
	 * @param NewAccountID The new AccountID of the user, or empty if the user logged out.
	 */
	static void LoginStatusChanged(FString NewAccountID);

private:
	enum class EAccountSource
	{
		EFromRegistry,
		EFromOnlineSubsystem,
	};

	/** Private helper for setting the UserID. Assumes the instance is valid. Not to be used by external code. */
	static void PrivateSetUserID(const FString& AccountID, EAccountSource AccountSource);

	static bool bIsInitialized;
	static TSharedPtr<IAnalyticsProvider> Analytics;
	static FString CurrentAccountID;
	static EAccountSource CurrentAccountSource;
};