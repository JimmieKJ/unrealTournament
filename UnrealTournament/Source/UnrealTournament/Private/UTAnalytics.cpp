// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "UTAnalytics.h"
#include "Runtime/Launch/Resources/Version.h"
#include "PerfCountersHelpers.h"
#include "QosInterface.h"
#if WITH_QOSREPORTER
	#include "QoSReporter.h"
#endif // WITH_QOSREPORTER

DEFINE_LOG_CATEGORY(LogUTAnalytics);

TArray<FString> FUTAnalytics::GenericParamNames;

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
				ConfigMap.Add(TEXT("APIServerET"), TEXT("https://datarouter.ol.epicgames.com/"));
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

	InitializeAnalyticParameterNames();

	bIsInitialized = true;
}

void FUTAnalytics::InitializeAnalyticParameterNames()
{
	//Generic STATS
	GenericParamNames.Reserve(EGenericAnalyticParam::NUM_GENERIC_PARAMS);
	GenericParamNames.SetNum(EGenericAnalyticParam::NUM_GENERIC_PARAMS);


	// server stats
#define AddGenericParamName(ParamName) \
	GenericParamNames[EGenericAnalyticParam::ParamName] = TEXT(#ParamName)

	AddGenericParamName(MatchTime);
	AddGenericParamName(MapName);

	AddGenericParamName(Hostname);
	AddGenericParamName(SystemId);
	AddGenericParamName(InstanceId);
	AddGenericParamName(BuildVersion);
	AddGenericParamName(MachinePhysicalRAMInGB);
	AddGenericParamName(NumLogicalCoresAvailable);
	AddGenericParamName(NumPhysicalCoresAvailable);
	AddGenericParamName(MachineCPUSignature);
	AddGenericParamName(MachineCPUBrandName);
	AddGenericParamName(NumClients);
	
	AddGenericParamName(Platform);
	AddGenericParamName(Location);
	AddGenericParamName(SocialPartyCount);
	AddGenericParamName(IsIdle);
	AddGenericParamName(RegionId);
	AddGenericParamName(PlayerContextLocationPerMinute);

	AddGenericParamName(HitchThresholdInMs);
	AddGenericParamName(NumHitchesAboveThreshold);
	AddGenericParamName(TotalUnplayableTimeInMs);
	AddGenericParamName(ServerUnplayableCondition);

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

FString FUTAnalytics::GetGenericParamName(EGenericAnalyticParam::Type InGenericParam)
{
	if (GIsEditor)
	{
		if (InGenericParam < GenericParamNames.Num())
		{
			return GenericParamNames[InGenericParam];
		}
		return FString();
	}
	else
	{
		check(InGenericParam < GenericParamNames.Num());
		return GenericParamNames[InGenericParam];
	}
}

void FUTAnalytics::SetMatchInitialParameters(AUTGameMode* UTGM, TArray<FAnalyticsEventAttribute>& ParamArray, bool bNeedMatchTime)
{
	if (bNeedMatchTime && UTGM)
	{
		ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::MatchTime), GetMatchTime(UTGM)));
	}
	FString MapName = GetMapName(UTGM);
	ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::MapName), MapName));
}

void FUTAnalytics::SetServerInitialParameters(TArray<FAnalyticsEventAttribute>& ParamArray)
{
	ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::Hostname), FPlatformProcess::ComputerName()));
	ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::SystemId), FPlatformMisc::GetOperatingSystemId()));
#if WITH_QOSREPORTER
	ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::InstanceId), FQoSReporter::GetQoSReporterInstanceId()));
#endif
	ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::BuildVersion), ENGINE_VERSION_STRING));

	// include some machine and CPU info for easier tracking
	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::MachinePhysicalRAMInGB), MemoryStats.TotalPhysicalGB));
	ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::NumLogicalCoresAvailable), FPlatformMisc::NumberOfCoresIncludingHyperthreads()));
	ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::NumPhysicalCoresAvailable), FPlatformMisc::NumberOfCores()));
	ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::MachineCPUSignature), FPlatformMisc::GetCPUInfo()));
	ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::MachineCPUBrandName), FPlatformMisc::GetCPUBrand()));

#if USE_SERVER_PERF_COUNTERS
	// include information about number of clients in all server events
	ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::NumClients), PerfCountersGet(TEXT("NumClients"), 0)));
#endif

}

FString FUTAnalytics::GetPlatform()
{
	FString PlayerPlatform = FString();
	if (PLATFORM_WINDOWS)
	{
		if (PLATFORM_64BITS)
		{
			PlayerPlatform = "Win64";
		}
		else
		{
			PlayerPlatform = "Win32";
		}
	}
	else if (PLATFORM_LINUX)
	{
		PlayerPlatform = "Linux";
	}
	else if (PLATFORM_MAC)
	{
		PlayerPlatform = "Mac";
	}
	else
	{
		PlayerPlatform = "Unknown";
	}

	return PlayerPlatform;
}

int32 FUTAnalytics::GetMatchTime(AUTPlayerController* UTPC)
{
	int32 MatchTime = 0;
	if (UTPC && UTPC->GetWorld())
	{
		if (AUTGameState* UTGameState = Cast<AUTGameState>(UTPC->GetWorld()->GetGameState()))
		{
			MatchTime = UTGameState->GetServerWorldTimeSeconds();
		}
	}

	return MatchTime;
}

int32 FUTAnalytics::GetMatchTime(AUTGameMode* UTGM)
{
	int32 MatchTime = 0;
	if (UTGM)
	{
		AUTGameState* UTGameState = Cast<AUTGameState>(UTGM->GameState);
		if (UTGameState)
		{
			MatchTime = UTGameState->GetServerWorldTimeSeconds();
		}
	}

	return MatchTime;
}


FString FUTAnalytics::GetMapName(AUTPlayerController* UTPC)
{
	FString MapName;
	if (UTPC && UTPC->GetLevel() && UTPC->GetLevel()->OwningWorld)
	{
		MapName = UTPC->GetLevel()->OwningWorld->GetMapName();
	}

	return MapName;
}

FString FUTAnalytics::GetMapName(AUTGameMode* UTGM)
{
	FString MapName;
	if (UTGM && UTGM->GetLevel() && UTGM->GetLevel()->OwningWorld)
	{
		MapName = UTGM->GetLevel()->OwningWorld->GetMapName();
	}

	return MapName;
}

/*
* @EventName ServerUnplayableCondition
*
* @Trigger Fires during the match (on the server) when server reports an unplayable condition.
*
* @Type Sent by the server for the match
*
* @EventParam MatchTime int32 The match time when this event fired
* @EventParam MapName string The name of the played map
* @EventParam Hostname string Machine network name (hostname), can also be used to identify VM in the cloud
* @EventParam SystemId string Unique Id of an operating system install (essentially means "machine id" unless the OS is reinstalled between runs)
* @EventParam InstanceId string Unique Id of a single server instance (stays the same between matches for the whole lifetime of the server)
* @EventParam EngineVersion string Engine version string
* @EventParam MachinePhysicalRAMInGB int32 Total physical memory on the machine in GB
* @EventParam NumLogicalCoresAvailable int32 Number of logical cores available to the process on the machine
* @EventParam NumPhysicalCoresAvailable int32 Number of physical cores available to the process on the machine
* @EventParam MachineCPUSignature int32 CPU signature (CPUID with EAX=1, i.e. family, model, stepping combined).
* @EventParam MachineCPUBrandName string CPU brand string.
* @EventParam NumClients int32 Instantaneous number of clients

* @EventParam HitchThresholdInMs float Unplayable hitch threshold (in ms) as currently configured.
* @EventParam NumHitchesAboveThreshold int32 Number of hitches above the unplayable threshold
* @EventParam TotalUnplayableTimeInMs float Total unplayable time (in ms) as current

* @Comments Reports about an unplayable condition on the server.
*/
void FUTAnalytics::FireEvent_ServerUnplayableCondition(AUTGameMode* UTGM, double HitchThresholdInMs, int32 NumHitchesAboveThreshold, double TotalUnplayableTimeInMs)
{
	if (LIKELY(UTGM))
	{
		const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider = GetProviderPtr();
		if (AnalyticsProvider.IsValid())
		{
			AUTGameState* GameState = UTGM->GetGameState<AUTGameState>();
			if (LIKELY(GameState))
			{
				TArray<FAnalyticsEventAttribute> ParamArray;
				SetMatchInitialParameters(UTGM, ParamArray, true);
				SetServerInitialParameters(ParamArray);

				ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::HitchThresholdInMs), HitchThresholdInMs));
				ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::NumHitchesAboveThreshold), NumHitchesAboveThreshold));
				ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::TotalUnplayableTimeInMs), TotalUnplayableTimeInMs));

				AnalyticsProvider->RecordEvent(GetGenericParamName(EGenericAnalyticParam::ServerUnplayableCondition), ParamArray);
#if WITH_QOSREPORTER
				if (FQoSReporter::IsAvailable())
				{
					FQoSReporter::GetProvider().RecordEvent(GetGenericParamName(EGenericAnalyticParam::ServerUnplayableCondition), ParamArray);
				}
#endif //WITH_QOSREPORTER
			}
		}
	}
}

/*
* @EventName PlayerContextLocationPerMinute
*
* @Trigger Fires every minute with the players location
*
* @Type Sent by the client
*
* @EventParam Platform int32 The platform the client is on 0=PC / 1=PS4
* @EventParam Location string The context location of the player
* @EventParam SocialPartyCount int32 The number of people in a players social party (counts the player as a party member)
* @EventParam RegionId the region reported by the user (automatic from ping, or self selected in settings)
*
* @Comments
*/
void FUTAnalytics::FireEvent_PlayerContextLocationPerMinute(AUTPlayerController* UTPC, FString& PlayerContextLocation, const int32 NumSocialPartyMembers)
{
	if (UTPC)
	{
		const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider = GetProviderPtr();
		if (AnalyticsProvider.IsValid())
		{
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::Platform), GetPlatform()));
			ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::Location), PlayerContextLocation));
			ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::SocialPartyCount), NumSocialPartyMembers));
			ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::RegionId), FQosInterface::Get()->GetRegionId()));
			AnalyticsProvider->RecordEvent(GetGenericParamName(EGenericAnalyticParam::PlayerContextLocationPerMinute), ParamArray);
		}
	}
}