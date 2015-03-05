// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "UTAnalytics.h"

bool FUTAnalytics::bIsInitialized = false;
TSharedPtr<IAnalyticsProvider> FUTAnalytics::Analytics = NULL;
// initialize to a dummy value to ensure the first time we set the AccountID it detects it as a change.
FString FUTAnalytics::CurrentAccountID(TEXT("__UNINITIALIZED__"));
FUTAnalytics::EAccountSource FUTAnalytics::CurrentAccountSource = FUTAnalytics::EAccountSource::EFromRegistry;

/**
 * On-demand construction of the singleton. 
 */
IAnalyticsProvider& FUTAnalytics::GetProvider()
{
	checkf(bIsInitialized && Analytics.IsValid(), TEXT("FUTAnalytics::GetProvider called outside of Initialize/Shutdown."));
	return *Analytics.Get();
}

TSharedPtr<IAnalyticsProvider> FUTAnalytics::GetProviderPtr()
{
	checkf(bIsInitialized && Analytics.IsValid(), TEXT("FUTAnalytics::GetProvider called outside of Initialize/Shutdown."));
	return Analytics;
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
	// Set the UserID using the AccountID regkey if present.
	LoginStatusChanged(FString());

	bIsInitialized = true;
}


void FUTAnalytics::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	Analytics.Reset();
	bIsInitialized = false;
}

void FUTAnalytics::LoginStatusChanged(FString NewAccountID)
{
	if (IsAvailable())
	{
		// track the source of the account ID. If we get one, it's from OSS. Otherwise, we'll get it from the registry.
		EAccountSource AccountSource = EAccountSource::EFromOnlineSubsystem;
		// empty AccountID tells us we are logged out, and get the cached one from the registry.
		if (NewAccountID.IsEmpty())
		{
			NewAccountID = FPlatformMisc::GetEpicAccountId();
			AccountSource = EAccountSource::EFromRegistry;
		}
		// Restart the session if the AccountID or AccountSource is changing. This prevents spuriously creating new sessions.
		// We restart the session if the AccountSource is changing because it is part of the UserID. This way the analytics
		// system will know that our source for the AccountID is different, even if the ID itself is the same.
		if (NewAccountID != CurrentAccountID || AccountSource != CurrentAccountSource)
		{
			// this will do nothing if a session is not already started.
			Analytics->EndSession();
			PrivateSetUserID(NewAccountID, AccountSource);
			Analytics->StartSession();
		}
	}
}

void FUTAnalytics::PrivateSetUserID(const FString& AccountID, EAccountSource AccountSource)
{
	// Set the UserID to "MachineID|AccountID|OSID|AccountIDSource".
	const TCHAR* AccountSourceStr = AccountSource == EAccountSource::EFromRegistry ? TEXT("Reg") : TEXT("OSS");
	Analytics->SetUserID(FString::Printf(TEXT("%s|%s|%s|%s"), *FPlatformMisc::GetMachineId().ToString(EGuidFormats::Digits).ToLower(), *AccountID, *FPlatformMisc::GetOperatingSystemId(), AccountSourceStr));
	// remember the current value so we don't spuriously restart the session if the user logs in later with the same ID.
	CurrentAccountID = AccountID;
	CurrentAccountSource = AccountSource;
}
