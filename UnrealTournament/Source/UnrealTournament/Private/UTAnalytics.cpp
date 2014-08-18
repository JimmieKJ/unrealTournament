// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "UTAnalytics.h"

bool FUTAnalytics::bIsInitialized = false;
TSharedPtr<IAnalyticsProvider> FUTAnalytics::Analytics = NULL;

/**
 * On-demand construction of the singleton. 
 */
IAnalyticsProvider& FUTAnalytics::GetProvider()
{
	checkf(bIsInitialized && Analytics.IsValid(), TEXT("FUTAnalytics::GetProvider called outside of Initialize/Shutdown."));
	return *Analytics.Get();
}
 
void FUTAnalytics::Initialize()
{
	checkf(!bIsInitialized, TEXT("FUTAnalytics::Initialize called more than once."));

	// Setup some default engine analytics if there is nothing custom bound
	FAnalytics::FProviderConfigurationDelegate DefaultUTAnalyticsConfig;
	DefaultUTAnalyticsConfig.BindStatic( 
		[]( const FString& KeyName, bool bIsValueRequired ) -> FString
		{
			static TMap<FString, FString> ConfigMap;
			if (ConfigMap.Num() == 0)
			{
				ConfigMap.Add(TEXT("ProviderModuleName"), TEXT("AnalyticsET"));
				ConfigMap.Add(TEXT("APIServerET"), TEXT("http://etsource.epicgames.com/ET2/"));
				ConfigMap.Add(TEXT("APIKeyET"), TEXT("UnrealTournament.Release"));
			}

			FString* ConfigValue = ConfigMap.Find(KeyName);
			return ConfigValue != NULL ? *ConfigValue : TEXT("");
		} );

	// Connect the engine analytics provider (if there is a configuration delegate installed)
	Analytics = FAnalytics::Get().CreateAnalyticsProvider(
		FName(*DefaultUTAnalyticsConfig.Execute(TEXT("ProviderModuleName"), true)), 
		DefaultUTAnalyticsConfig);
	
	if (Analytics.IsValid())
	{
		Analytics->SetUserID(FPlatformMisc::GetUniqueDeviceId());
		Analytics->StartSession();
	}
	bIsInitialized = true;
}


void FUTAnalytics::Shutdown()
{
	checkf(bIsInitialized, TEXT("FUTAnalytics::Shutdown called outside of Initialize."));
	Analytics.Reset();
	bIsInitialized = false;
}
