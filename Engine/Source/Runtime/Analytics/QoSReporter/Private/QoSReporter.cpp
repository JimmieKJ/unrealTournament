// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "QoSReporterPrivatePCH.h"

#include "Core.h"
#include "Analytics.h"
#include "EngineVersion.h"
#include "QoSReporter.h"
#if WITH_PERFCOUNTERS
	#include "PerfCountersModule.h"
#endif // WITH_PERFCOUNTERS
#include "PerfCountersHelpers.h"

#ifndef WITH_QOSREPORTER
	#error "WITH_QOSREPORTER should be defined in Build.cs file"
#endif

bool FQoSReporter::bIsInitialized;
TSharedPtr<IAnalyticsProvider> FQoSReporter::Analytics;
FGuid FQoSReporter::InstanceId;

/**
* External code should bind this delegate if QoS reporting is desired,
* preferably in private code that won't be redistributed.
*/
QOSREPORTER_API FAnalytics::FProviderConfigurationDelegate& GetQoSOverrideConfigDelegate()
{
	static FAnalytics::FProviderConfigurationDelegate Delegate;
	return Delegate;
}


/**
* Get analytics pointer
*/
IAnalyticsProvider& FQoSReporter::GetProvider()
{
	checkf(bIsInitialized && IsAvailable(), TEXT("FQoSReporter::GetProvider called outside of Initialize/Shutdown."));

	return *Analytics.Get();
}

void FQoSReporter::Initialize()
{
	checkf(!bIsInitialized, TEXT("FQoSReporter::Initialize called more than once."));

	// allow overriding from configs
	bool bEnabled = true;
	if (GConfig->GetBool(TEXT("QoSReporter"), TEXT("bEnabled"), bEnabled, GEngineIni) && !bEnabled)
	{
		UE_LOG(LogQoSReporter, Verbose, TEXT("QoSReporter disabled by config setting"));
		return;
	}

	// Setup some default engine analytics if there is nothing custom bound
	FAnalytics::FProviderConfigurationDelegate DefaultEngineAnalyticsConfig;
	DefaultEngineAnalyticsConfig.BindLambda(
		[=](const FString& KeyName, bool bIsValueRequired) -> FString
	{
		static TMap<FString, FString> ConfigMap;
		if (ConfigMap.Num() == 0)
		{
			ConfigMap.Add(TEXT("ProviderModuleName"), TEXT("QoSReporter"));
			ConfigMap.Add(TEXT("APIKeyQoS"), FString::Printf(TEXT("%s.%s"), FApp::GetGameName(), FAnalytics::ToString(FAnalytics::Get().GetBuildType())));
		}

		// Check for overrides
		if (GetQoSOverrideConfigDelegate().IsBound())
		{
			const FString OverrideValue = GetQoSOverrideConfigDelegate().Execute(KeyName, bIsValueRequired);
			if (!OverrideValue.IsEmpty())
			{
				return OverrideValue;
			}
		}

		FString* ConfigValue = ConfigMap.Find(KeyName);
		return ConfigValue != NULL ? *ConfigValue : TEXT("");
	});

	// Connect the engine analytics provider (if there is a configuration delegate installed)
	Analytics = FAnalytics::Get().CreateAnalyticsProvider(
		FName(*DefaultEngineAnalyticsConfig.Execute(TEXT("ProviderModuleName"), true)),
		DefaultEngineAnalyticsConfig);

	// add a unique id
	InstanceId = FGuid::NewGuid();
	FPlatformMisc::CreateGuid(InstanceId);

	// check if Configs override the heartbeat interval
	
	float ConfigHeartbeatInterval = 0.0;
	if (GConfig->GetFloat(TEXT("QoSReporter"), TEXT("HeartbeatInterval"), ConfigHeartbeatInterval, GEngineIni))
	{
		HeartbeatInterval = ConfigHeartbeatInterval;
		UE_LOG(LogQoSReporter, Verbose, TEXT("HeartbeatInterval configured to %f from config."), HeartbeatInterval);
	}

	// randomize heartbeats to prevent servers from bursting at once (they hit rate limit on data router and get throttled with 429)
	LastHeartbeatTimestamp = FPlatformTime::Seconds() + HeartbeatInterval * FMath::FRand();

	bIsInitialized = true;
}

void FQoSReporter::Shutdown()
{
	checkf(!bIsInitialized || Analytics.IsValid(), TEXT("Analytics provider for QoS reporter module is left initialized - internal error."));

	Analytics.Reset();
	bIsInitialized = false;
}

FGuid FQoSReporter::GetQoSReporterInstanceId()
{
	return InstanceId;
}

double FQoSReporter::ModuleInitializationTime = FPlatformTime::Seconds();
bool FQoSReporter::bStartupEventReported = false;

void FQoSReporter::ReportStartupCompleteEvent()
{
	if (bStartupEventReported || !Analytics.IsValid())
	{
		return;
	}

	double CurrentTime = FPlatformTime::Seconds();
	double Difference = CurrentTime - ModuleInitializationTime;

	TArray<FAnalyticsEventAttribute> ParamArray;
	ParamArray.Add(FAnalyticsEventAttribute(EQoSEvents::ToString(EQoSEventParam::StartupTime), Difference));
	Analytics->RecordEvent(EQoSEvents::ToString(EQoSEventParam::StartupTime), ParamArray);

	bStartupEventReported = true;
}

double FQoSReporter::HeartbeatInterval = 300;
double FQoSReporter::LastHeartbeatTimestamp = 0;
extern ENGINE_API float GAverageFPS;

void FQoSReporter::Tick()
{
	if (!Analytics.IsValid())
	{
		return;
	}

	static double PreviousTickTime = FPlatformTime::Seconds();
	double CurrentTime = FPlatformTime::Seconds();

	if (HeartbeatInterval > 0 && CurrentTime - LastHeartbeatTimestamp > HeartbeatInterval)
	{
		SendHeartbeat();
		LastHeartbeatTimestamp = CurrentTime;
	}

	// detect too long pauses between ticks
	const double kTooLongPauseBetweenTicksInSeconds = 0.25f;	// 4 fps	- correct perfcounter name below if changing this value and make sure analytics is on the same page
	const double DeltaBetweenTicks = CurrentTime - PreviousTickTime;
	static int TimesHappened = 0;
	if (DeltaBetweenTicks > kTooLongPauseBetweenTicksInSeconds)
	{
		++TimesHappened;

		PerfCountersIncrement(TEXT("HitchesAbove250msec"));

		UE_LOG(LogQoSReporter, Warning, TEXT("QoS reporter could not tick for %f sec (longer than threshold of %f sec) - happened %d time(s), average FPS is %f. Sending heartbeats might have been affected"),
			DeltaBetweenTicks, kTooLongPauseBetweenTicksInSeconds, TimesHappened, GAverageFPS);
	}

	PreviousTickTime = CurrentTime;
}

void FQoSReporter::SendHeartbeat()
{
	checkf(Analytics.IsValid(), TEXT("SendHeartbeat() should not be called if Analytics provider was not configured"));

	TArray<FAnalyticsEventAttribute> ParamArray;
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("Role"), GetApplicationRole()));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("SystemId"), FPlatformMisc::GetOperatingSystemId()));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("InstanceId"), InstanceId.ToString()));

	if (IsRunningDedicatedServer())
	{
		AddServerHeartbeatAttributes(ParamArray);
		Analytics->RecordEvent(EQoSEvents::ToString(EQoSEventParam::ServerPerfCounters), ParamArray);
	}
	else
	{
		AddClientHeartbeatAttributes(ParamArray);
		Analytics->RecordEvent(EQoSEvents::ToString(EQoSEventParam::Heartbeat), ParamArray);
	}
}

/**
 * Sends heartbeat stats.
 */
void FQoSReporter::AddServerHeartbeatAttributes(TArray<FAnalyticsEventAttribute> & OutArray)
{
#if WITH_PERFCOUNTERS
	IPerfCounters* PerfCounters = IPerfCountersModule::Get().GetPerformanceCounters();
	if (PerfCounters)
	{
		const TMap<FString, IPerfCounters::FJsonVariant>& PerfCounterMap = PerfCounters->GetAllCounters();
		int NumAddedCounters = 0;
		// we want to skip reporting average FPS if there are no human players
		bool bShouldSkipAverageFPS = (PerfCounters->Get("NumClients", 0) == 0);

		// only add string and number values
		for (const auto& It : PerfCounterMap)
		{
			// do not report average FPS unless there are actual people playing
			if (bShouldSkipAverageFPS && It.Key == TEXT("AverageFPS"))
			{
				continue;
			}

			const IPerfCounters::FJsonVariant& JsonValue = It.Value;
			switch (JsonValue.Format)
			{
				case IPerfCounters::FJsonVariant::String:
					OutArray.Add(FAnalyticsEventAttribute(It.Key, JsonValue.StringValue));
					break;
				case IPerfCounters::FJsonVariant::Number:
					OutArray.Add(FAnalyticsEventAttribute(It.Key, FString::Printf(TEXT("%f"), JsonValue.NumberValue)));
					break;
				default:
					// don't write anything (supporting these requires more changes in PerfCounters API)
					UE_LOG(LogQoSReporter, Log, TEXT("PerfCounter '%s' of unsupported type %d skipped"), *It.Key, static_cast<int32>(JsonValue.Format));
					break;
			}
		}

		// for compatibility with wash, avoid resetting if -statsport is used (temporary measure to avoid interference)
		static bool bCheckedResettingStats = false;
		static bool bResetStats = true;

		if (!bCheckedResettingStats)
		{
			int32 StatsPort = -1;
			if (FParse::Value(FCommandLine::Get(), TEXT("statsPort="), StatsPort) && StatsPort > 0)
			{
				bResetStats = false;
			}

			bCheckedResettingStats = true;
		}
		
		if (bResetStats)
		{
			UE_LOG(LogQoSReporter, Verbose, TEXT("Resetting PerfCounters - new stat period begins."));
			PerfCounters->ResetStatsForNextPeriod();
		}
		else
		{
			UE_LOG(LogQoSReporter, Verbose, TEXT("Not resetting PerfCounters, relying on wash to drive stats periods."));
		}
	}
	else if (UE_SERVER)
	{
		UE_LOG(LogQoSReporter, Warning, TEXT("PerfCounters module is not available, could not send proper server heartbeat."));
	}
#else
	#if UE_SERVER
		#error "QoS module requires perfcounters for servers"
	#endif // UE_SERVER
#endif // WITH_PERFCOUNTERS
}

void FQoSReporter::AddClientHeartbeatAttributes(TArray<FAnalyticsEventAttribute> & OutArray)
{
	OutArray.Add(FAnalyticsEventAttribute(TEXT("AverageFPS"), GAverageFPS));
}

/**
 * Returns application role (server, client)
 */
FString FQoSReporter::GetApplicationRole()
{
	if (IsRunningDedicatedServer())
	{
		static FString DedicatedServer(TEXT("DedicatedServer"));
		return DedicatedServer;
	}
	else if (IsRunningClientOnly())
	{
		static FString ClientOnly(TEXT("ClientOnly"));
		return ClientOnly;
	}
	else if (IsRunningGame())
	{
		static FString StandaloneGame(TEXT("StandaloneGame"));
		return StandaloneGame;
	}

	static FString Editor(TEXT("Editor"));
	return Editor;
}

