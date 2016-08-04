// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameMode.h"
#include "UTGameState.h"
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

	AddGenericParamName(Team);
	AddGenericParamName(MaxRequiredTextureSize);
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

void FUTAnalytics::SetInitialParameters(AUTPlayerController* UTPC, TArray<FAnalyticsEventAttribute>& ParamArray, bool bNeedMatchTime)
{
	if (UTPC)
	{
		if (bNeedMatchTime == true)
		{
			ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::MatchTime), GetMatchTime(UTPC)));
		}

		FString MapName = GetMapName(UTPC);
		const int32 Team = UTPC->GetTeamNum();
		
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(UTPC->PlayerState);
		
		ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::MapName), MapName));
		ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::Platform), GetPlatform()));
		ParamArray.Add(FAnalyticsEventAttribute(GetGenericParamName(EGenericAnalyticParam::Team), Team));
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
* @EventName UTFPSCharts
*
* @Trigger Fires at the end of the match with the FPS Charts stats for each player
*
* @Type Sent by client
*
* @EventParam MapName string The name of the played map
* @EventParam PlaylistId int32 The playlist of the current match (4=PvP, 5=Coop, 6=Solo)
* @EventParam Bucket_%i_%i_TimePercentage float The percentage of time that the FPS amount was between x - y
* @EventParam Hitch_%i_%i_HitchCount int32 The number of hitches between x - y milliseconds long
* @EventParam Hitch_%i_%i_HitchTime float The time spent in hitchy frames that lasted between x - y milliseconds long
* @EventParam TotalHitches int32 The total number of hitches
* @EventParam TotalGameBoundHitches	int32 The total number of game thread bound hitches
* @EventParam TotalRenderBoundHitches int32 The total number of render thread bound hitches
* @EventParam TotalGPUBoundHitches int32 The total number of gpu bound hitches
* @EventParam TotalTimeInHitchFrames float The total time spent in all hitch buckets
* @EventParam PercentSpentHitching float The percentage of time spent hitching (has the desired frame time subtracted out of hitch frames before computing)
* @EventParam HitchesPerMinute float The avg. number of hitches per minute played
* @EventParam ChangeList string The change list that was played
* @EventParam BuildType string The games build type that was played
* @EventParam DateStamp string The date stamp that the FPS charts stats took place
* @EventParam Platform string The platform this build was played on
* @EventParam OS string The OS of the client
* @EventParam CPU string The CPU of the client
* @EventParam DesktopGPU string The desktop GPU of the client (may not be teh one we end up using for rendering, see GPUAdapter)
* @EventParam ResolutionQuality float The resolution quality of the client
* @EventParam ViewDistanceQuality int32 The view distance quality of the client
* @EventParam AntiAliasingQuality int32 The anti-aliasing quality of the client
* @EventParam ShadowQuality int32 The shadow quality of the client
* @EventParam PostProcessQuality int32 The post process quality of the client
* @EventParam TextureQuality int32 The texture quality of the client
* @EventParam FXQuality int32 The effects quality of the client
* @EventParam AvgFPS float The average fps of the client
* @EventParam PercentAbove30 float The time percentage when the fps was above 30
* @EventParam PercentAbove60 float The time percentage when the fps was above 60
* @EventParam PercentAbove120 float The time percentage when the fps was above 120
* @EventParam MVP30 float The estimated percentage of missed vsyncs at a target framerate of 30
* @EventParam MVP60 float The estimated percentage of missed vsyncs at a target framerate of 60
* @EventParam MVP120 float The estimated percentage of missed vsyncs at a target framerate of 120
* @EventParam TimeDisregarded float The time that the FPS chart didn't count anywhere
* @EventParam Time float The amount of time FPS Charts was capturing data
* @EventParam FrameCount float The total number of frames
* @EventParam AvgGPUTime float The average time the GPU took to render the frame
* @EventParam PercentGameThreadBound float The percentage of game thread bound frames
* @EventParam PercentRenderThreadBound float The percentage of render thread bound frames
* @EventParam PercentGPUBound float The percentage of GPU bound frames
* @EventParam TotalPhysical The total amount of CPU physical memory detected
* @EventParam TotalVirtual The total amount of CPU virtual memory detected
* @EventParam VRAM The amount of VRAM detected
* @EventParam VSYS The amound of video system memory detected
* @EventParam VSHR The amount of shared video memory detected
* @EventParam CPU_NumCoresP The number of physical CPU cores detected
* @EventParam CPU_NumCoresL The number of logical CPU cores detected (e.g., hyperthreading)
* @EventParam GPUAdapter The GPU adapter string we actually created the D3D11 device for
* @EventParam GPUVendorID The vendor ID for the GPU adapter
* @EventParam GPUDeviceID The device ID for the GPU adapter
* @EventParam GPUDriverVerI The internal driver version string for the GPU
* @EventParam GPUDriverVerU The user facing driver version string for the GPU
* @EventParam CPUBM The last cached value of the CPU benchmark result
* @EventParam GPUBM The last cached value of the GPU benchmark result
* @EventParam ScreenPct The screen percentage for 3D rendering
* @EventParam WindowMode The fullscreen/windowing mode
* @EventParam SizeX The width of the display screen
* @EventParam SizeY The height of the display screen
* @EventParam VSync Whether or not VSync is enabled
* @EventParam FrameRateLimit The frame rate limit
* @EventParam MaxRequiredTextureSize The maximum required texture memory size recorded
*
* @Comments
*/
void FUTAnalytics::FireEvent_UTFPSCharts(AUTPlayerController* UTPC, TArray<FAnalyticsEventAttribute>& InParamArray)
{
	if (UTPC)
	{
		const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider = GetProviderPtr();
		if (AnalyticsProvider.IsValid())
		{
			SetInitialParameters(UTPC, InParamArray, false);
			AnalyticsProvider->RecordEvent(GetGenericParamName(EGenericAnalyticParam::UTFPSCharts), InParamArray);
		}
	}
}

/*
* @EventName UTServerFPSCharts
*
* @Trigger Fires at the end of the match with the FPS Charts stats for the server
*
* @Type Sent by the server for the match
*
* @EventParam MapName string The name of the played map
* @EventParam Bucket_%i_%i_TimePercentage float The percentage of time that the FPS amount was between x - y
* @EventParam Hitch_%i_%i_HitchCount int32 The number of hitches between x - y milliseconds long
* @EventParam Hitch_%i_%i_HitchTime float The time spent in hitchy frames that lasted between x - y milliseconds long
* @EventParam TotalHitches int32 The total number of hitches
* @EventParam TotalGameBoundHitches	int32 The total number of game thread bound hitches
* @EventParam TotalRenderBoundHitches int32 The total number of render thread bound hitches
* @EventParam TotalGPUBoundHitches int32 The total number of gpu bound hitches
* @EventParam TotalTimeInHitchFrames float The total time spent in all hitch buckets
* @EventParam PercentSpentHitching float The percentage of time spent hitching (has the desired frame time subtracted out of hitch frames before computing)
* @EventParam HitchesPerMinute float The avg. number of hitches per minute played
* @EventParam ChangeList string The change list that was played
* @EventParam BuildType string The games build type that was played
* @EventParam DateStamp string The date stamp that the FPS charts stats took place
* @EventParam Platform string The platform this build was played on
* @EventParam OS string The OS of the server
* @EventParam CPU string The CPU of the server
* @EventParam GPU string The GPU of the server
* @EventParam ResolutionQuality float The resolution quality of the server
* @EventParam ViewDistanceQuality int32 The view distance quality of the server
* @EventParam AntiAliasingQuality int32 The anti-aliasing quality of the server
* @EventParam ShadowQuality int32 The shadow quality of the client
* @EventParam PostProcessQuality int32 The post process quality of the server
* @EventParam TextureQuality int32 The texture quality of the server
* @EventParam FXQuality int32 The effects quality of the server
* @EventParam AvgFPS float The average fps of the server
* @EventParam PercentAbove30 float The time percentage when the fps was above 30
* @EventParam PercentAbove60 float The time percentage when the fps was above 60
* @EventParam PercentAbove120 float The time percentage when the fps was above 120
* @EventParam MVP30 float The estimated percentage of missed vsyncs at a target framerate of 30
* @EventParam MVP60 float The estimated percentage of missed vsyncs at a target framerate of 60
* @EventParam MVP120 float The estimated percentage of missed vsyncs at a target framerate of 120
* @EventParam TimeDisregarded float The time that the FPS chart didn't count anywhere
* @EventParam Time float The amount of time FPS Charts was capturing data
* @EventParam FrameCount float The total number of frames
* @EventParam AvgGPUTime float The average time the GPU took to render the frame
* @EventParam PercentGameThreadBound float The percentage of game thread bound frames
* @EventParam PercentRenderThreadBound float The percentage of render thread bound frames
* @EventParam PercentGPUBound float The percentage of GPU bound frames
*
* @Comments
*/
void FUTAnalytics::FireEvent_UTServerFPSCharts(AUTGameMode* UTGM, TArray<FAnalyticsEventAttribute>& InParamArray)
{
	if (UTGM)
	{
		SetMatchInitialParameters(UTGM, InParamArray, false);

		const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider = GetProviderPtr();
		if (AnalyticsProvider.IsValid())
		{
			AnalyticsProvider->RecordEvent(GetGenericParamName(EGenericAnalyticParam::UTServerFPSCharts), InParamArray);
		}

#if WITH_QOSREPORTER
		if (FQoSReporter::IsAvailable())
		{
			FQoSReporter::GetProvider().RecordEvent(GetGenericParamName(EGenericAnalyticParam::UTServerFPSCharts), InParamArray);
		}
#endif

	}
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
	if (UTGM)
	{
		const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider = GetProviderPtr();
		if (AnalyticsProvider.IsValid())
		{
			AUTGameState* GameState = UTGM->GetGameState<AUTGameState>();
			if (GameState)
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