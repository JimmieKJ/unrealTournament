// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateViewerApp.h"
#include "RequiredProgramMainCPPInclude.h"
#include "STestSuite.h"
#include "ISourceCodeAccessModule.h"
#include "SPerfSuite.h"
#include "SDockTab.h"
#include "SWebBrowser.h"

IMPLEMENT_APPLICATION(SlateViewer, "SlateViewer");

#define LOCTEXT_NAMESPACE "SlateViewer"

namespace WorkspaceMenu
{
	TSharedRef<FWorkspaceItem> DeveloperMenu = FWorkspaceItem::NewGroup(LOCTEXT("DeveloperMenu", "Developer"));
}


void RunSlateViewer( const TCHAR* CommandLine )
{
	// start up the main loop
	GEngineLoop.PreInit(CommandLine);

	// crank up a normal Slate application using the platform's standalone renderer
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());

	// Load the source code access module
	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>( FName( "SourceCodeAccess" ) );
	
	// Manually load in the source code access plugins, as standalone programs don't currently support plugins.
#if PLATFORM_MAC
	IModuleInterface& XCodeSourceCodeAccessModule = FModuleManager::LoadModuleChecked<IModuleInterface>( FName( "XCodeSourceCodeAccess" ) );
	SourceCodeAccessModule.SetAccessor(FName("XCodeSourceCodeAccess"));
#elif PLATFORM_WINDOWS
	IModuleInterface& VisualStudioSourceCodeAccessModule = FModuleManager::LoadModuleChecked<IModuleInterface>( FName( "VisualStudioSourceCodeAccess" ) );
	SourceCodeAccessModule.SetAccessor(FName("VisualStudioSourceCodeAccess"));
#endif

	// set the application name
	FGlobalTabmanager::Get()->SetApplicationTitle(LOCTEXT("AppTitle", "Slate Viewer"));
	FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").RegisterTabSpawner(WorkspaceMenu::DeveloperMenu);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner("WebBrowserTab", FOnSpawnTab::CreateStatic(&SpawnWebBrowserTab))
		.SetDisplayName(LOCTEXT("WebBrowserTab", "Web Browser"));
	
	if (FParse::Param(FCommandLine::Get(), TEXT("perftest")))
	{
		// Bring up perf test
		SummonPerfTestSuite();
	}
	else
	{
		// Bring up the test suite.
		RestoreSlateTestSuite();
	}


#if WITH_SHARED_POINTER_TESTS
	SharedPointerTesting::TestSharedPointer<ESPMode::Fast>();
	SharedPointerTesting::TestSharedPointer<ESPMode::ThreadSafe>();
#endif

	// loop while the server does the rest
	while (!GIsRequestingExit)
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FStats::AdvanceFrame(false);
		FTicker::GetCoreTicker().Tick(FApp::GetDeltaTime());
		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();		
		FPlatformProcess::Sleep(0);
	}

	FSlateApplication::Shutdown();
}

TSharedRef<SDockTab> SpawnWebBrowserTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("WebBrowserTab", "Web Browser"))
		.ToolTipText(LOCTEXT("WebBrowserTabToolTip", "Switches to the Web Browser to test its features."))
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SWebBrowser)
			.ParentWindow(Args.GetOwnerWindow())
		];
}

#undef LOCTEXT_NAMESPACE
