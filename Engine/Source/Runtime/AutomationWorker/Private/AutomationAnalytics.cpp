// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AutomationWorkerPrivatePCH.h"
#include "AutomationAnalytics.h"
#include "EngineBuildSettings.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "GeneralProjectSettings.h"



bool FAutomationAnalytics::bIsInitialized;
TSharedPtr<IAnalyticsProvider> FAutomationAnalytics::Analytics;
TArray<FString> FAutomationAnalytics::AutomationEventNames;
TArray<FString> FAutomationAnalytics::AutomationParamNames;
FString FAutomationAnalytics::MachineSpec;

/**
* Automation analytics config log to initialize the automation analytics provider.
* External code should bind this delegate if automation analytics are desired,
* preferably in private code that won't be redistributed.
*/
FAnalytics::FProviderConfigurationDelegate& GetAutomationAnalyticsOverrideConfigDelegate()
{
	static FAnalytics::FProviderConfigurationDelegate Delegate;
	return Delegate;
}


/**
* Get analytics pointer
*/
IAnalyticsProvider& FAutomationAnalytics::GetProvider()
{
	checkf(bIsInitialized && IsAvailable(), TEXT("FAutomationAnalytics::GetProvider called outside of Initialize/Shutdown."));

	return *Analytics.Get();
}

void FAutomationAnalytics::Initialize()
{
	checkf(!bIsInitialized, TEXT("FAutomationAnalytics::Initialize called more than once."));

	// this will only be true for builds that have editor support (currently PC, Mac, Linux)
	// The idea here is to only send editor events for actual editor runs, not for things like -game runs of the editor.
	//const bool bIsEditorRun = WITH_EDITOR && GIsEditor && !IsRunningCommandlet();
	const bool bIsEditorRun = GIsEditor;

	// We also want to identify a real run of a game, which is NOT necessarily the opposite of an editor run.
	// Ideally we'd be able to tell explicitly, but with content-only games, it becomes difficult.
	// So we ensure we are not an editor run, we don't have EDITOR stuff compiled in, we are not running a commandlet,
	// we are not a generic, utility program, and we require cooked data.
	//const bool bIsGameRun = !WITH_EDITOR && !IsRunningCommandlet() && !FPlatformProperties::IsProgram() && FPlatformProperties::RequiresCookedData();

	const bool bIsGameRun = !GIsEditor;

	const bool bShouldInitAnalytics = bIsEditorRun || bIsGameRun;

	if (bShouldInitAnalytics)
	{
		// Setup some default automation analytics if there is nothing custom bound
		FAnalytics::FProviderConfigurationDelegate DefaultAutomationAnalyticsConfig;
		DefaultAutomationAnalyticsConfig.BindLambda(
			[=](const FString& KeyName, bool bIsValueRequired) -> FString
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
					!FEngineBuildSettings::IsInternalBuild();   // Internal Epic build
				const TCHAR* BuildTypeStr = bUseReleaseAccount ? TEXT("Release") : TEXT("Dev");
				const TCHAR* UE4TypeStr = FRocketSupport::IsRocket() ? TEXT("Rocket") : FEngineBuildSettings::IsPerforceBuild() ? TEXT("Perforce") : TEXT("UnrealEngine");
				if (bIsEditorRun)
				{
					ConfigMap.Add(TEXT("APIKeyET"), FString::Printf(TEXT("UEEditor.Automation.%s.%s"), UE4TypeStr, BuildTypeStr));
				}
				else
				{
					const TCHAR* GameName = GInternalGameName;
					ConfigMap.Add(TEXT("APIKeyET"), FString::Printf(TEXT("UEGame.Automation.%s.%s.%s"), UE4TypeStr, BuildTypeStr, GameName));
				}
			}

			// Check for overrides
			if (GetAutomationAnalyticsOverrideConfigDelegate().IsBound())
			{
				const FString OverrideValue = GetAutomationAnalyticsOverrideConfigDelegate().Execute(KeyName, bIsValueRequired);
				if (!OverrideValue.IsEmpty())
				{
					return OverrideValue;
				}
			}

			FString* ConfigValue = ConfigMap.Find(KeyName);
			return ConfigValue != NULL ? *ConfigValue : TEXT("");
		});

		Analytics = FAnalytics::Get().CreateAnalyticsProvider(
			FName(*DefaultAutomationAnalyticsConfig.Execute(TEXT("ProviderModuleName"), true)),
			DefaultAutomationAnalyticsConfig);
		if (Analytics.IsValid())
		{
			Analytics->SetUserID(FString::Printf(TEXT("%s|%s|%s"), *FPlatformMisc::GetMachineId().ToString(EGuidFormats::Digits).ToLower(), *FPlatformMisc::GetEpicAccountId(), *FPlatformMisc::GetOperatingSystemId()));

			TArray<FAnalyticsEventAttribute> StartSessionAttributes;
			// Add project info whether we are in editor or game.
			FString GameName = GInternalGameName;
			StartSessionAttributes.Emplace(TEXT("ProjectName"), GameName);


			//Need a way to get these without using the Editor
			/*StartSessionAttributes.Emplace(TEXT("ProjectID"), ProjectSettings.ProjectID);
			StartSessionAttributes.Emplace(TEXT("ProjectDescription"), ProjectSettings.Description);
			StartSessionAttributes.Emplace(TEXT("ProjectVersion"), ProjectSettings.ProjectVersion);*/

			Analytics->StartSession(StartSessionAttributes);

			MachineSpec = FParse::Param(FCommandLine::Get(), TEXT("60hzmin")) 
				? TEXT("60hzmin")
				: FParse::Param(FCommandLine::Get(), TEXT("30hzmin")) 
				? TEXT("30hzmin") 
				: TEXT("");
		}
	}
	if (Analytics.IsValid())
	{
		bIsInitialized = true;
		InititalizeAnalyticParameterNames();
	}
}

void FAutomationAnalytics::Shutdown()
{
	Analytics.Reset();
	bIsInitialized = false;
}

FString FAutomationAnalytics::GetAutomationEventName(EAutomationEventName::Type InEventName)
{
	check(InEventName < AutomationEventNames.Num());
	return AutomationEventNames[InEventName];
}

FString FAutomationAnalytics::GetAutomationParamName(EAutomationAnalyticParam::Type InEngineParam)
{
	check(InEngineParam < AutomationParamNames.Num());
	return AutomationParamNames[InEngineParam];
}

void FAutomationAnalytics::InititalizeAnalyticParameterNames()
{
	// Engine Event Names
	AutomationEventNames.Reserve(EAutomationEventName::NUM_ENGINE_EVENT_NAMES);
	AutomationEventNames.SetNum(EAutomationEventName::NUM_ENGINE_EVENT_NAMES);

	AutomationEventNames[EAutomationEventName::FPSCapture] = TEXT("FPSCapture");
	AutomationEventNames[EAutomationEventName::AutomationTestResults] = TEXT("AutomationTest");

	for (int32 EventIndex = 0; EventIndex < EAutomationEventName::NUM_ENGINE_EVENT_NAMES; EventIndex++)
	{
		if (AutomationEventNames[EventIndex].IsEmpty())
		{
			UE_LOG(LogAutomationAnalytics, Error, TEXT("Engine Events missing a event: %d!"), EventIndex);
		}
	}

	// Engine Event Params
	AutomationParamNames.Reserve(EAutomationAnalyticParam::NUM_ENGINE_PARAMS);
	AutomationParamNames.SetNum(EAutomationAnalyticParam::NUM_ENGINE_PARAMS);

	AutomationParamNames[EAutomationAnalyticParam::MapName] = TEXT("MapName");
	AutomationParamNames[EAutomationAnalyticParam::MatineeName] = TEXT("MatineeName");
	AutomationParamNames[EAutomationAnalyticParam::TimeStamp] = TEXT("TimeStamp");
	AutomationParamNames[EAutomationAnalyticParam::Platform] = TEXT("Platform");
	AutomationParamNames[EAutomationAnalyticParam::Spec] = TEXT("Spec");
	AutomationParamNames[EAutomationAnalyticParam::CL] = TEXT("CL");
	AutomationParamNames[EAutomationAnalyticParam::FPS] = TEXT("FPS");
	AutomationParamNames[EAutomationAnalyticParam::BuildConfiguration] = TEXT("BuildConfiguration");
	AutomationParamNames[EAutomationAnalyticParam::AverageFrameTime] = TEXT("AverageFrameTime");
	AutomationParamNames[EAutomationAnalyticParam::AverageGameThreadTime] = TEXT("AverageGameThreadTime");
	AutomationParamNames[EAutomationAnalyticParam::AverageRenderThreadTime] = TEXT("AverageRenderThreadTime");
	AutomationParamNames[EAutomationAnalyticParam::AverageGPUTime] = TEXT("AverageGPUTime");
	AutomationParamNames[EAutomationAnalyticParam::PercentOfFramesAtLeast30FPS] = TEXT("PercentOfFramesAtLeast30FPS");
	AutomationParamNames[EAutomationAnalyticParam::PercentOfFramesAtLeast60FPS] = TEXT("PercentOfFramesAtLeast60FPS");

	AutomationParamNames[EAutomationAnalyticParam::TestName] = TEXT("TestName");
	AutomationParamNames[EAutomationAnalyticParam::BeautifiedName] = TEXT("BeautifiedName");
	AutomationParamNames[EAutomationAnalyticParam::ExecutionCount] = TEXT("ExecutionCount");
	AutomationParamNames[EAutomationAnalyticParam::IsSuccess] = TEXT("IsSuccess");
	AutomationParamNames[EAutomationAnalyticParam::Duration] = TEXT("Duration");
	AutomationParamNames[EAutomationAnalyticParam::ErrorCount] = TEXT("ErrorCount");
	AutomationParamNames[EAutomationAnalyticParam::WarningCount] = TEXT("WarningCount");

	for (int32 ParamIndex = 0; ParamIndex < EAutomationAnalyticParam::NUM_ENGINE_PARAMS; ParamIndex++)
	{
		if (AutomationParamNames[ParamIndex].IsEmpty())
		{
			UE_LOG(LogAutomationAnalytics, Error, TEXT("Engine Events missing a param: %d!"), ParamIndex);
		}
	}
}

//Timestamp,Platform
void FAutomationAnalytics::SetInitialParameters(TArray<FAnalyticsEventAttribute>& ParamArray)
{
	int32 TimeStamp;
	FString PlatformName;

	TimeStamp = FApp::GetCurrentTime();
	PlatformName = FPlatformProperties::PlatformName();

	ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::TimeStamp), TimeStamp));
	ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::Platform), PlatformName));
}


/*
* @EventName FPSCapture
*
* @Trigger 
*
* @Type Static
*
* @EventParam TimeStamp int32 The time in seconds when the test when the capture ended
* @EventParam Platform string The platform in which the test is run on
* @EventParam CL string The Changelist for the build
* @EventParam Spec string The spec of the machine running the test
* @EventParam MapName string The map that the test was run on
* @EventParam MatineeName string The name of the matinee that fired the event
* @EventParam FPS string 
* @EventParam BuildConfiguration string Debug/Development/Test/Shipping
* @EventParam AverageFrameTime string Time for a frame in ms
* @EventParam AverageGameThreadTime string Time for the game thread in ms
* @EventParam AverageRenderThreadTime string Time for the rendering thread in ms
* @EventParam AverageGPUTime string Time for the GPU to flush in ms
* @EventParam PercentOfFramesAtLeast30FPS string 
* @EventParam PercentOfFramesAtLeast60FPS string 
*
* @Comments 
*
*/
void FAutomationAnalytics::FireEvent_FPSCapture(const FAutomationPerformanceSnapshot& PerfSnapshot)
{
	if (Analytics.IsValid())
	{
		TArray<FAnalyticsEventAttribute> ParamArray;
		SetInitialParameters(ParamArray);
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::CL), PerfSnapshot.Changelist));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::Spec), MachineSpec));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::MapName), PerfSnapshot.MapName));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::MatineeName), PerfSnapshot.MatineeName));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::FPS), PerfSnapshot.AverageFPS));

		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::BuildConfiguration), PerfSnapshot.BuildConfiguration));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::AverageFrameTime), PerfSnapshot.AverageFrameTime));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::AverageGameThreadTime), PerfSnapshot.AverageGameThreadTime));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::AverageRenderThreadTime), PerfSnapshot.AverageRenderThreadTime));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::AverageGPUTime), PerfSnapshot.AverageGPUTime));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::PercentOfFramesAtLeast30FPS), PerfSnapshot.PercentOfFramesAtLeast30FPS));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::PercentOfFramesAtLeast60FPS), PerfSnapshot.PercentOfFramesAtLeast60FPS));

		Analytics->RecordEvent(GetAutomationEventName(EAutomationEventName::FPSCapture), ParamArray);
	}
}

/*
* @EventName AutomationTestResults
*
* @Trigger Fires when an automation test has completed
*
* @Type Static
*
* @EventParam TimeStamp int32 The time in seconds when the test ended
* @EventParam Platform string The platform in which the test is run on
* @EventParam CL string The Changelist for the build
* @EventParam TestName string The execution string of the test being run
* @EventParam BeautifiedTestName string The name displayed in the UI
* @EventParam ExecutionCount string
* @EventParam IsSuccess bool Where the test was a success or not
* @EventParam Duration float The duration the test took to complete
* @EventParam ErrorCount int32 The amount of errors during the test
* @EventParam WarningCount int32 The amount of warnings during the test
* @EventParam CL string The Changelist for the build
*
* @Comments 
*
*/
void FAutomationAnalytics::FireEvent_AutomationTestResults(const FAutomationWorkerRunTestsReply* TestResults, const FString& BeautifiedTestName)
{
	if (Analytics.IsValid())
	{
		TArray<FAnalyticsEventAttribute> ParamArray;
		SetInitialParameters(ParamArray);
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::TestName), TestResults->TestName));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::BeautifiedName), BeautifiedTestName));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::ExecutionCount), TestResults->ExecutionCount));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::IsSuccess), TestResults->Success));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::Duration), TestResults->Duration));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::ErrorCount), TestResults->Errors.Num()));
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::WarningCount), TestResults->Warnings.Num()));

		const int32 ChangeList = FEngineVersion::Current().GetChangelist();
		FString ChangeListString = FString::FromInt(ChangeList);
		ParamArray.Add(FAnalyticsEventAttribute(GetAutomationParamName(EAutomationAnalyticParam::CL), ChangeListString));

		Analytics->RecordEvent(GetAutomationEventName(EAutomationEventName::AutomationTestResults), ParamArray);
	}
}

bool FAutomationAnalytics::IsInitialized()
{
	return bIsInitialized;
}
