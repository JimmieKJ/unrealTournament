// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EngineAnalytics.h"
#include "EngineBuildSettings.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "GeneralProjectSettings.h"

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

	check(GEngine);

	// this will only be true for builds that have editor support (currently PC, Mac, Linux)
	// The idea here is to only send editor events for actual editor runs, not for things like -game runs of the editor.
	const bool bIsEditorRun = WITH_EDITOR && GIsEditor && !IsRunningCommandlet();

	// We also want to identify a real run of a game, which is NOT necessarily the opposite of an editor run.
	// Ideally we'd be able to tell explicitly, but with content-only games, it becomes difficult.
	// So we ensure we are not an editor run, we don't have EDITOR stuff compiled in, we are not running a commandlet,
	// we are not a generic, utility program, and we require cooked data.
	const bool bIsGameRun = !WITH_EDITOR && !IsRunningCommandlet() && !FPlatformProperties::IsProgram() && FPlatformProperties::RequiresCookedData();

	const bool bShouldInitAnalytics = bIsEditorRun || bIsGameRun;

	// Outside of the editor, the only engine analytics usage is the hardware survey
	bShouldSendUsageEvents = bIsEditorRun 
		? GEngine->AreEditorAnalyticsEnabled() 
		: bIsGameRun 
			? GEngine->bHardwareSurveyEnabled 
			: false;

	if (bShouldInitAnalytics)
	{
		{
			// Setup some default engine analytics if there is nothing custom bound
			FAnalytics::FProviderConfigurationDelegate DefaultEngineAnalyticsConfig;
			DefaultEngineAnalyticsConfig.BindLambda( 
				[=]( const FString& KeyName, bool bIsValueRequired ) -> FString
				{
					static TMap<FString, FString> ConfigMap;
					if (ConfigMap.Num() == 0)
					{
						ConfigMap.Add(TEXT("ProviderModuleName"), TEXT("AnalyticsET"));
						ConfigMap.Add(TEXT("APIServerET"), TEXT("http://etsource.epicgames.com/ET2/"));

						// We always use the "Release" analytics account unless we're running in analytics test mode (usually with
						// a command-line parameter), or we're an internal Epic build
						const FAnalytics::BuildType AnalyticsBuildType = FAnalytics::Get().GetBuildType();
						const bool bUseReleaseAccount =
							(AnalyticsBuildType == FAnalytics::Development || AnalyticsBuildType == FAnalytics::Release) &&
							!FEngineBuildSettings::IsInternalBuild();	// Internal Epic build
						const TCHAR* BuildTypeStr = bUseReleaseAccount ? TEXT("Release") : TEXT("Dev");
						const TCHAR* UE4TypeStr = FRocketSupport::IsRocket() ? TEXT("Rocket") : FEngineBuildSettings::IsPerforceBuild() ? TEXT("Perforce") : TEXT("UnrealEngine");
						if (bIsEditorRun)
						{
							ConfigMap.Add(TEXT("APIKeyET"), FString::Printf(TEXT("UEEditor.%s.%s"), UE4TypeStr, BuildTypeStr));
						}
						else
						{
							const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
							ConfigMap.Add(TEXT("APIKeyET"), FString::Printf(TEXT("UEGame.%s.%s|%s|%s"), UE4TypeStr, BuildTypeStr, *ProjectSettings.ProjectID.ToString(), *ProjectSettings.ProjectName));
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
				// Add project info whether we are in editor or game.
				const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
				StartSessionAttributes.Emplace(TEXT("ProjectName"), ProjectSettings.ProjectName);
				StartSessionAttributes.Emplace(TEXT("ProjectID"), ProjectSettings.ProjectID);
				StartSessionAttributes.Emplace(TEXT("ProjectDescription"), ProjectSettings.Description);
				StartSessionAttributes.Emplace(TEXT("ProjectVersion"), ProjectSettings.ProjectVersion);

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
