// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"
#include "EngineVersion.h"
#include "CrashDescription.h"
#include "CrashReportAnalytics.h"
#include "QoSReporter.h"

#if !CRASH_REPORT_UNATTENDED_ONLY
	#include "SCrashReportClient.h"
	#include "CrashReportClient.h"
	#include "CrashReportClientStyle.h"
	#include "ISlateReflectorModule.h"
#endif // !CRASH_REPORT_UNATTENDED_ONLY

#include "CrashReportClientUnattended.h"
#include "RequiredProgramMainCPPInclude.h"
#include "AsyncWork.h"

#include "MainLoopTiming.h"

#include "PlatformErrorReport.h"
#include "XmlFile.h"

/** Default main window size */
const FVector2D InitialWindowDimensions(740, 560);

/** Average tick rate the app aims for */
const float IdealTickRate = 30.f;

/** Set this to true in the code to open the widget reflector to debug the UI */
const bool RunWidgetReflector = false;

IMPLEMENT_APPLICATION(CrashReportClient, "CrashReportClient");
DEFINE_LOG_CATEGORY(CrashReportClientLog);

/** Directory containing the report */
static FString ReportDirectoryAbsolutePath;

/** Name of the game passed via the command line. */
static FString GameNameFromCmd;

/**
 * Look for the report to upload, either in the command line or in the platform's report queue
 */
void ParseCommandLine(const TCHAR* CommandLine)
{
	const TCHAR* CommandLineAfterExe = FCommandLine::RemoveExeName(CommandLine);

	// Use the first argument if present and it's not a flag
	if (*CommandLineAfterExe)
	{
		TArray<FString> Switches;
		TArray<FString> Tokens;
		TMap<FString, FString> Params;
		{
			FString NextToken;
			while (FParse::Token(CommandLineAfterExe, NextToken, false))
			{
				if (**NextToken == TCHAR('-'))
				{
					new(Switches)FString(NextToken.Mid(1));
				}
				else
				{
					new(Tokens)FString(NextToken);
				}
			}

			for (int32 SwitchIdx = Switches.Num() - 1; SwitchIdx >= 0; --SwitchIdx)
			{
				FString& Switch = Switches[SwitchIdx];
				TArray<FString> SplitSwitch;
				if (2 == Switch.ParseIntoArray(SplitSwitch, TEXT("="), true))
				{
					Params.Add(SplitSwitch[0], SplitSwitch[1].TrimQuotes());
					Switches.RemoveAt(SwitchIdx);
				}
			}
		}

		if (Tokens.Num() > 0)
		{
			ReportDirectoryAbsolutePath = Tokens[0];
		}

		GameNameFromCmd = Params.FindRef("AppName");
	}

	if (ReportDirectoryAbsolutePath.IsEmpty())
	{
		ReportDirectoryAbsolutePath = FPlatformErrorReport::FindMostRecentErrorReport();
	}
}

/**
 * Find the error report folder and check it matches the app name if provided
 */
FPlatformErrorReport LoadErrorReport()
{
	if (ReportDirectoryAbsolutePath.IsEmpty())
	{
		UE_LOG(CrashReportClientLog, Warning, TEXT("No error report found"));
		return FPlatformErrorReport();
	}

	FPlatformErrorReport ErrorReport(ReportDirectoryAbsolutePath);

	FString Filename;
	// CrashContext.runtime-xml has the precedence over the WER
	if (ErrorReport.FindFirstReportFileWithExtension( Filename, *FGenericCrashContext::CrashContextExtension ))
	{
		FPrimaryCrashProperties::Set( new FCrashContext( ReportDirectoryAbsolutePath / Filename ) );
	}
	else if (ErrorReport.FindFirstReportFileWithExtension( Filename, TEXT( ".xml" ) ))
	{
		FPrimaryCrashProperties::Set( new FCrashWERContext( ReportDirectoryAbsolutePath / Filename ) );
	}
	else
	{
		UE_LOG(CrashReportClientLog, Warning, TEXT("No error report found"));
		return FPlatformErrorReport();
	}

#if CRASH_REPORT_UNATTENDED_ONLY
	return ErrorReport;
#else
	if (!GameNameFromCmd.IsEmpty() && GameNameFromCmd != FPrimaryCrashProperties::Get()->GameName)
	{
		// Don't display or upload anything if it's not the report we expected
		ErrorReport = FPlatformErrorReport();
	}
	return ErrorReport;
#endif
}


void RunCrashReportClient(const TCHAR* CommandLine)
{
	// Override the stack size for the thread pool.
	FQueuedThreadPool::OverrideStackSize = 256 * 1024;

	// Set up the main loop
	GEngineLoop.PreInit(CommandLine);

	// Initialize config.
	FCrashReportClientConfig::Get();

	const bool bUnattended = 
#if CRASH_REPORT_UNATTENDED_ONLY
		true;
#else
		FApp::IsUnattended();
#endif // CRASH_REPORT_UNATTENDED_ONLY

	// Set up the main ticker
	FMainLoopTiming MainLoop(IdealTickRate, bUnattended ? EMainLoopOptions::CoreTickerOnly : EMainLoopOptions::UsingSlate);

	// Find the report to upload in the command line arguments
	ParseCommandLine(CommandLine);

	// Increase the HttpSendTimeout to 5 minutes
	GConfig->SetFloat(TEXT("HTTP"), TEXT("HttpSendTimeout"), 5*60.0f, GEngineIni);

	FPlatformErrorReport::Init();
	auto ErrorReport = LoadErrorReport();
	
	if (ErrorReport.HasFilesToUpload() && FPrimaryCrashProperties::Get() != nullptr)
	{
		FCrashReportAnalytics::Initialize();
		FQoSReporter::Initialize();
		FQoSReporter::SetBackendDeploymentName(FPrimaryCrashProperties::Get()->DeploymentName);

		if (bUnattended)
		{
			// In the unattended mode we don't send any PII.
			FCrashReportClientUnattended CrashReportClient(ErrorReport);
			ErrorReport.SetUserComment(NSLOCTEXT("CrashReportClient", "UnattendedMode", "Sent in the unattended mode"));

			// loop until the app is ready to quit
			while (!GIsRequestingExit)
			{
				MainLoop.Tick();
			}
		}
		else
		{
#if !CRASH_REPORT_UNATTENDED_ONLY
			// crank up a normal Slate application using the platform's standalone renderer
			FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());

			// Prepare the custom Slate styles
			FCrashReportClientStyle::Initialize();

			// Create the main implementation object
			TSharedRef<FCrashReportClient> CrashReportClient = MakeShareable(new FCrashReportClient(ErrorReport));

			// open up the app window	
			TSharedRef<SCrashReportClient> ClientControl = SNew(SCrashReportClient, CrashReportClient);

			auto Window = FSlateApplication::Get().AddWindow(
				SNew(SWindow)
				.Title(NSLOCTEXT("CrashReportClient", "CrashReportClientAppName", "Unreal Engine 4 Crash Reporter"))
				.HasCloseButton(FCrashReportClientConfig::Get().IsAllowedToCloseWithoutSending())
				.ClientSize(InitialWindowDimensions)
				[
					ClientControl
				]);

			Window->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride::CreateSP(CrashReportClient, &FCrashReportClient::RequestCloseWindow));

			// Setting focus seems to have to happen after the Window has been added
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);

			// Debugging code
			if (RunWidgetReflector)
			{
				FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").DisplayWidgetReflector();
			}

			// loop until the app is ready to quit
			while (!GIsRequestingExit)
			{
				MainLoop.Tick();

				if (CrashReportClient->ShouldWindowBeHidden())
				{
					Window->HideWindow();
				}
			}

			// Clean up the custom styles
			FCrashReportClientStyle::Shutdown();

			// Close down the Slate application
			FSlateApplication::Shutdown();
#endif // !CRASH_REPORT_UNATTENDED_ONLY
		}

		// Shutdown analytics.
		FCrashReportAnalytics::Shutdown();
		FQoSReporter::Shutdown();
	}

	FPrimaryCrashProperties::Shutdown();
	FPlatformErrorReport::ShutDown();

	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FTaskGraphInterface::Shutdown();

	FEngineLoop::AppExit();
}
