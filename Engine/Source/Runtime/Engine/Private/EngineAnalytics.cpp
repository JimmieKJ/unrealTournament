// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EngineAnalytics.h"
#include "EngineBuildSettings.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"


bool FEngineAnalytics::bIsInitialized;
TSharedPtr<IAnalyticsProvider> FEngineAnalytics::Analytics;

#define DEBUG_ANALYTICS 0

#if DEBUG_ANALYTICS

DEFINE_LOG_CATEGORY_STATIC(LogDebugAnalytics, Log, All);

namespace DebugAnalyticsProviderDefs
{
	const static int32 MaxAttributeCount = 40;
}

/**
 * Debug analytics provider for debugging analytics events
 * without the need to connect to a real provider.
 */
class FDebugAnalyticsProvider : public IAnalyticsProvider
{
public:
	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
	{
		UE_LOG(LogDebugAnalytics, Log, TEXT("FDebugAnalyticsProvider: StartSession..."));
		OutputAttributesToLog(Attributes);
		return true;
	}

	virtual void EndSession()
	{
		UE_LOG(LogDebugAnalytics, Log, TEXT("FDebugAnalyticsProvider: EndSession"));
	}

	virtual FString GetSessionID() const
	{
		return SessionID;
	}

	virtual bool SetSessionID(const FString& InSessionID)
	{
		UE_LOG(LogDebugAnalytics, Log, TEXT("FDebugAnalyticsProvider: SetSessionID SessionID=%s"), *InSessionID);
		SessionID = InSessionID;
		return true;
	}

	virtual void FlushEvents()
	{
	}

	virtual void SetUserID(const FString& InUserID)
	{
		UE_LOG(LogDebugAnalytics, Log, TEXT("FDebugAnalyticsProvider: SetUserID UserID=%s"), *InUserID);
		UserID = InUserID;
	}

	virtual FString GetUserID() const
	{
		return UserID;
	}

	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
	{
		if (Attributes.Num() > DebugAnalyticsProviderDefs::MaxAttributeCount)
		{
			UE_LOG(LogDebugAnalytics, Warning, TEXT("FDebugAnalyticsProvider: Event %s has too many attributes (%d)"), *EventName, Attributes.Num());
		}

		UE_LOG(LogDebugAnalytics, Log, TEXT("FDebugAnalyticsProvider: RecordEvent Name=%s"), *EventName);
		OutputAttributesToLog(Attributes);
	}

private:
	void OutputAttributesToLog(const TArray<FAnalyticsEventAttribute>& Attributes)
	{
		for (auto& Attrib : Attributes)
		{
			UE_LOG(LogDebugAnalytics, Log, TEXT("AnalyticsAttrib Name=%s, Value=%s"), *Attrib.AttrName, *Attrib.AttrValue);
		}
	}

	FString SessionID;
	FString UserID;
};

#endif // DEBUG_ANALYTICS

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
	checkf(bIsInitialized && Analytics.IsValid(), TEXT("FEngineAnalytics::GetProvider called outside of Initialize/Shutdown."));
	return *Analytics.Get();
}
 
void FEngineAnalytics::Initialize()
{
	checkf(!bIsInitialized, TEXT("FEngineAnalytics::Initialize called more than once."));

	// Never use analytics when running a commandlet tool
	bool bShouldInitAnalytics = !IsRunningCommandlet();

	check(GEngine);
#if WITH_EDITORONLY_DATA
	if (GIsEditor)
	{
		bShouldInitAnalytics = bShouldInitAnalytics && GEngine->bEditorAnalyticsEnabled;
	}
	else
#endif
	{
		// Outside of the editor, the only engine analytics usage is the hardware survey
		bShouldInitAnalytics = bShouldInitAnalytics && GEngine->bHardwareSurveyEnabled;
	}

	if (bShouldInitAnalytics)
	{
#if DEBUG_ANALYTICS
		// Dummy analytics provider just logs events locally
		Analytics = MakeShareable(new FDebugAnalyticsProvider());
		Analytics->SetUserID(FPlatformMisc::GetUniqueDeviceId());
		Analytics->StartSession();
#else

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
				Analytics->SetUserID(FPlatformMisc::GetUniqueDeviceId());
				Analytics->StartSession();
			}
		}
#endif
	}
	bIsInitialized = true;
}


void FEngineAnalytics::Shutdown()
{
	Analytics.Reset();
	bIsInitialized = false;
}
