// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealTemplate.h"

class IAnalyticsProvider;

class FUTAnalytics : FNoncopyable
{
public:
	static UNREALTOURNAMENT_API IAnalyticsProvider& GetProvider();
	
	/** Helper function to determine if the provider is valid. */
	static UNREALTOURNAMENT_API bool IsAvailable() 
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