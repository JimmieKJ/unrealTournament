// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EngineAnalytics.h"
#include "EngineBuildSettings.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

bool FEngineAnalytics::bIsInitialized;
TSharedPtr<IAnalyticsProvider> FEngineAnalytics::Analytics;
bool FEngineAnalytics::bShouldSendUsageEvents;

/**
 * Engine analytics config log to initialize the engine analytics provider.
 * External code should bind this delegate if engine analytics are desired,
 * preferably in private code that won't be redistributed.
 */
FAnalytics::FProviderConfigurationDelegate& GetEngineAnalyticsOverrideConfigDelegate()
{
	static FAnalytics::FProviderConfigurationDelegate Delegate;
	return Delegate;
}


/**
 * Get analytics pointer
 */
IAnalyticsProvider& FEngineAnalytics::GetProvider()
{
	checkf(bIsInitialized && IsAvailable(), TEXT("FEngineAnalytics::GetProvider called outside of Initialize/Shutdown."));

	return *Analytics.Get();
}
 
void FEngineAnalytics::Initialize()
{
	checkf(!bIsInitialized, TEXT("FEngineAnalytics::Initialize called more than once."));

	// Never use analytics when running a commandlet tool
	const bool bShouldInitAnalytics = !IsRunningCommandlet();

	check(GEngine);
#if WITH_EDITORONLY_DATA
	if (GIsEditor)
	{
		bShouldSendUsageEvents = GEngine->AreEditorAnalyticsEnabled();
	}
	else
#endif
	{
		// Outside of the editor, the only engine analytics usage is the hardware survey
		bShouldSendUsageEvents = GEngine->bHardwareSurveyEnabled;
	}

	if (bShouldInitAnalytics)
	{
		{
			// Setup some default engine analytics if there is nothing custom bound
			FAnalytics::FProviderConfigurationDelegate DefaultEngineAnalyticsConfig;
			DefaultEngineAnalyticsConfig.BindStatic( 
				[]( const FString& KeyName, bool bIsValueRequired ) -> FString
				{
					static TMap<FString, FString> ConfigMap;
					if (ConfigMap.Num() == 0)
					{
						ConfigMap.Add(TEXT("ProviderModuleName"), TEXT("AnalyticsET"));
						ConfigMap.Add(TEXT("APIServerET"), TEXT("http://etsource.epicgames.com/ET2/"));

						// We always use the "Release" analytics account unless we're running in analytics test mode (usually with
						// a command-line parameter), or we're an internal Epic build
						bool bUseReleaseAccount =
							(FAnalytics::Get().GetBuildType() == FAnalytics::Development ||  
							 FAnalytics::Get().GetBuildType() == FAnalytics::Release) &&
							!FEngineBuildSettings::IsInternalBuild();	// Internal Epic build

						if( FRocketSupport::IsRocket() )
						{
							const TCHAR* DevelopmentAccountAPIKeyET = TEXT("Rocket.Dev");
							const TCHAR* ReleaseAccountAPIKeyET = TEXT("Rocket.Release");
							ConfigMap.Add(TEXT("APIKeyET"), bUseReleaseAccount ? ReleaseAccountAPIKeyET : DevelopmentAccountAPIKeyET);
						}
						else if( FEngineBuildSettings::IsPerforceBuild() )
						{
							const TCHAR* DevelopmentAccountAPIKeyET = TEXT("Perforce.Dev");
							const TCHAR* ReleaseAccountAPIKeyET = TEXT("Perforce.Release");
							ConfigMap.Add(TEXT("APIKeyET"), bUseReleaseAccount ? ReleaseAccountAPIKeyET : DevelopmentAccountAPIKeyET);
						}
						else
						{
							const TCHAR* DevelopmentAccountAPIKeyET = TEXT("UnrealEngine.Dev");
							const TCHAR* ReleaseAccountAPIKeyET = TEXT("UnrealEngine.Release");
							ConfigMap.Add(TEXT("APIKeyET"), bUseReleaseAccount ? ReleaseAccountAPIKeyET : DevelopmentAccountAPIKeyET);
						}
					}

					// Check for overrides
					if( GetEngineAnalyticsOverrideConfigDelegate().IsBound() )
					{
						const FString OverrideValue = GetEngineAnalyticsOverrideConfigDelegate().Execute( KeyName, bIsValueRequired );
						if( !OverrideValue.IsEmpty() )
						{
							return OverrideValue;
						}
					}

					FString* ConfigValue = ConfigMap.Find(KeyName);
					return ConfigValue != NULL ? *ConfigValue : TEXT("");
				} );

			// Connect the engine analytics provider (if there is a configuration delegate installed)
			Analytics = FAnalytics::Get().CreateAnalyticsProvider(
				FName(*DefaultEngineAnalyticsConfig.Execute(TEXT("ProviderModuleName"), true)), 
				DefaultEngineAnalyticsConfig);
			if (Analytics.IsValid())
			{
				Analytics->SetUserID(FString::Printf(TEXT("%s|%s|%s"), *FPlatformMisc::GetMachineId().ToString(EGuidFormats::Digits).ToLower(), *FPlatformMisc::GetEpicAccountId(), *FPlatformMisc::GetOperatingSystemId()));

				TArray<FAnalyticsEventAttribute> StartSessionAttributes;
				GEngine->CreateStartupAnalyticsAttributes( StartSessionAttributes );

				Analytics->StartSession( StartSessionAttributes );
			}
		}
	}
	bIsInitialized = true;
}


void FEngineAnalytics::Shutdown()
{
	Analytics.Reset();
	bIsInitialized = false;
}
