// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"

#include "CrashReportAnalytics.h"
#include "EngineBuildSettings.h"
#include "Runtime/Analytics/AnalyticsET/Public/AnalyticsET.h"

// WARNING! Copied from EngineAnalytics.cpp

bool FCrashReportAnalytics::bIsInitialized;
TSharedPtr<IAnalyticsProvider> FCrashReportAnalytics::Analytics;

/**
 * Default config func that essentially tells the crash reporter to disable analytics.
 */
FAnalyticsET::Config DefaultAnalyticsConfigFunc()
{
	return FAnalyticsET::Config();
}

/**
* Engine analytics config to initialize the crash reporter analytics provider.
* External code should bind this delegate if crash reporter analytics are desired,
* preferably in private code that won't be redistributed.
*/
TFunction<FAnalyticsET::Config()>& GetCrashReportAnalyticsConfigFunc()
{
	static TFunction<FAnalyticsET::Config()> Config;
	return Config;
}

/**
 * On-demand construction of the singleton. 
 */
IAnalyticsProvider& FCrashReportAnalytics::GetProvider()
{
	checkf(bIsInitialized && Analytics.IsValid(), TEXT("FCrashReportAnalytics::GetProvider called outside of Initialize/Shutdown."));
	return *Analytics.Get();
}
 
void FCrashReportAnalytics::Initialize()
{
	checkf(!bIsInitialized, TEXT("FCrashReportAnalytics::Initialize called more than once."));

	FAnalyticsET::Config Config = GetCrashReportAnalyticsConfigFunc()();
	if (!Config.APIServerET.IsEmpty())
	{
		// Connect the engine analytics provider (if there is a configuration delegate installed)
		Analytics = FAnalyticsET::Get().CreateAnalyticsProvider(Config);
		if( Analytics.IsValid() )
		{
			Analytics->SetUserID(FString::Printf(TEXT("%s|%s|%s"), *FPlatformMisc::GetMachineId().ToString(EGuidFormats::Digits).ToLower(), *FPlatformMisc::GetEpicAccountId(), *FPlatformMisc::GetOperatingSystemId()));
			Analytics->StartSession();
		}
	}
	bIsInitialized = true;
}


void FCrashReportAnalytics::Shutdown()
{
	checkf(bIsInitialized, TEXT("FCrashReportAnalytics::Shutdown called outside of Initialize."));
	Analytics.Reset();
	bIsInitialized = false;
}
