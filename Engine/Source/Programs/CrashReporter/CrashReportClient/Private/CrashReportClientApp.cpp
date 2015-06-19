// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"
#include "CrashDescription.h"
#include "CrashReportAnalytics.h"

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
const FVector2D InitialWindowDimensions(640, 560);

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
		if (*CommandLineAfterExe != '-')
		{
			ReportDirectoryAbsolutePath = FParse::Token(CommandLineAfterExe, true /* handle escaped quotes */);
		}
		FParse::Value( CommandLineAfterExe, TEXT( "AppName=" ), GameNameFromCmd );
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
	
	FString XMLWerFilename;
	ErrorReport.FindFirstReportFileWithExtension( XMLWerFilename, TEXT( ".xml" ) );

	extern FCrashDescription& GetCrashDescription();
	GetCrashDescription() = FCrashDescription( ReportDirectoryAbsolutePath / XMLWerFilename );

#if CRASH_REPORT_UNATTENDED_ONLY
	return ErrorReport;
#else
	if( !GameNameFromCmd.IsEmpty() && GameNameFromCmd != GetCrashDescription().GameName )
	{
		// Don't display or upload anything if it's not the report we expected
		ErrorReport = FPlatformErrorReport();
	}
	return ErrorReport;
#endif
}

FCrashReportClientConfig::FCrashReportClientConfig()
	: DiagnosticsFilename( TEXT( "Diagnostics.txt" ))
{
	if( !GConfig->GetString( TEXT( "CrashReportClient" ), TEXT( "CrashReportReceiverIP" ), CrashReportReceiverIP, GEngineIni ) )
	{
		// Use the default value.
		CrashReportReceiverIP = TEXT( "http://crashreporter.epicgames.com:57005" );
	}

	UE_LOG( CrashReportClientLog, Log, TEXT( "CrashReportReceiverIP: %s" ), *CrashReportReceiverIP );
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
	
	if( ErrorReport.HasFilesToUpload() )
	{
		// Send analytics.
		extern FCrashDescription& GetCrashDescription();
		GetCrashDescription().SendAnalytics();
	}

	if (bUnattended)
	{
		ErrorReport.SetUserComment( NSLOCTEXT( "CrashReportClient", "UnattendedMode", "Sent in the unattended mode" ), false );
		FCrashReportClientUnattended CrashReportClient( ErrorReport );

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
			FSlateApplication::Get().AddWindow(
				SNew(SWindow)
				.ClientSize(FVector2D(800, 600))
				[
					FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").GetWidgetReflector()
				]);
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

	FPlatformErrorReport::ShutDown();

	FEngineLoop::AppPreExit();
	FTaskGraphInterface::Shutdown();

	FEngineLoop::AppExit();
}

void CrashReportClientCheck(bool bCondition, const TCHAR* Location)
{
	if (!bCondition)
	{
		UE_LOG(CrashReportClientLog, Warning, TEXT("CHECK FAILED at %s"), Location);
	}
}