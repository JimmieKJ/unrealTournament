// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnrealEd.cpp: UnrealEd package file
=============================================================================*/

#include "UnrealEd.h"
#include "AssetSelection.h"
#include "Factories.h"

#include "BlueprintUtilities.h"
#include "BlueprintGraphDefinitions.h"
#include "EdGraphUtilities.h"
#include "DebugToolExec.h"
#include "MainFrame.h"
#include "ISourceControlModule.h"
#include "EngineAnalytics.h"
#include "IAnalyticsProvider.h"

#include "GameProjectGenerationModule.h"
#include "AssetEditorManager.h"

#include "EditorActorFolders.h"
#include "UnrealEngine.h"

#include "IHeadMountedDisplay.h"
#include "IVREditorModule.h"

UUnrealEdEngine* GUnrealEd;

DEFINE_LOG_CATEGORY_STATIC(LogUnrealEd, Log, All);

/**
 * Provides access to the FEditorModeTools for the level editor
 */
class FEditorModeTools& GLevelEditorModeTools()
{
	static FEditorModeTools* EditorModeToolsSingleton = new FEditorModeTools;
	return *EditorModeToolsSingleton;
}

FLevelEditorViewportClient* GCurrentLevelEditingViewportClient = NULL;
/** Tracks the last level editing viewport client that received a key press. */
FLevelEditorViewportClient* GLastKeyLevelEditingViewportClient = NULL;

/**
 * Returns the path to the engine's editor resources directory (e.g. "/../../Engine/Content/Editor/")
 */
const FString GetEditorResourcesDir()
{
	return FPaths::Combine( FPlatformProcess::BaseDir(), *FPaths::EngineContentDir(), TEXT("Editor/") );
}

int32 EditorInit( IEngineLoop& EngineLoop )
{
	// Create debug exec.	
	GDebugToolExec = new FDebugToolExec;

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Editor Initialized"), STAT_EditorStartup, STATGROUP_LoadTime);
	
	FScopedSlowTask SlowTask(100, NSLOCTEXT("EngineLoop", "EngineLoop_Loading", "Loading..."));
	
	SlowTask.EnterProgressFrame(50);

	int32 ErrorLevel = EngineLoop.Init();
	if( ErrorLevel != 0 )
	{
		FPlatformSplash::Hide();
		return 0;
	}

	// Let the analytics know that the editor has started
	if ( FEngineAnalytics::IsAvailable() )
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MachineID"), FPlatformMisc::GetMachineId().ToString(EGuidFormats::Digits).ToLower()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("AccountID"), FPlatformMisc::GetEpicAccountId()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("GameName"), FApp::GetGameName()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("CommandLine"), FCommandLine::Get()));

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.ProgramStarted"), EventAttributes);
	}

	SlowTask.EnterProgressFrame(40);

	// Initialize the misc editor
	FUnrealEdMisc::Get().OnInit();
	
	SlowTask.EnterProgressFrame(10);

	// Prime our array of default directories for loading and saving content files to
	FEditorDirectories::Get().LoadLastDirectories();

	// Set up the actor folders singleton
	FActorFolders::Init();

	// =================== CORE EDITOR INIT FINISHED ===================

	// Hide the splash screen now that everything is ready to go
	FPlatformSplash::Hide();

	// Are we in immersive mode?
	const bool bIsImmersive = FPaths::IsProjectFilePathSet() && FParse::Param( FCommandLine::Get(), TEXT( "immersive" ) );

	// Do final set up on the editor frame and show it
	{
		// Tear down rendering thread once instead of doing it for every window being resized.
		SCOPED_SUSPEND_RENDERING_THREAD(true);

		// Startup Slate main frame and other editor windows
		{
			const bool bStartImmersive = bIsImmersive;
			const bool bStartPIE = bIsImmersive;

			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
			MainFrameModule.CreateDefaultMainFrame( bStartImmersive, bStartPIE );
		}
	}


	// Go straight to VR mode if we were asked to
	{
		if( !bIsImmersive && FParse::Param( FCommandLine::Get(), TEXT( "VREditor" ) ) )
		{
			IVREditorModule& VREditorModule = IVREditorModule::Get();
			VREditorModule.EnableVREditor( true );
		}
		else if( FParse::Param( FCommandLine::Get(), TEXT( "ForceVREditor" ) ) )
		{
			GEngine->DeferredCommands.Add( TEXT( "VREd.ForceVRMode" ) );
		}
	}

	// Check for automated build/submit option
	const bool bDoAutomatedMapBuild = FParse::Param( FCommandLine::Get(), TEXT("AutomatedMapBuild") );

	// Prompt to update the game project file to the current version, if necessary
	if ( FPaths::IsProjectFilePathSet() )
	{
		FGameProjectGenerationModule::Get().CheckForOutOfDateGameProjectFile();
		FGameProjectGenerationModule::Get().CheckAndWarnProjectFilenameValid();
	}

	// =================== EDITOR STARTUP FINISHED ===================
	
	// Stat tracking
	{
		const float StartupTime = (float)( FPlatformTime::Seconds() - GStartTime );

		if( FEngineAnalytics::IsAvailable() )
		{
			FEngineAnalytics::GetProvider().RecordEvent( 
				TEXT( "Editor.Performance.Startup" ), 
				TEXT( "Duration" ), FString::Printf( TEXT( "%.3f" ), StartupTime ) );
		}
	}

	FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("HierarchicalLODOutliner"));

	// this will be ultimately returned from main(), so no error should be 0.
	return 0;
}

void EditorExit()
{
	GLevelEditorModeTools().SetDefaultMode(FBuiltinEditorModes::EM_Default);
	GLevelEditorModeTools().DeactivateAllModes(); // this also activates the default mode

	// Save out any config settings for the editor so they don't get lost
	GEditor->SaveConfig();
	GLevelEditorModeTools().SaveConfig();

	// Clean up the actor folders singleton
	FActorFolders::Cleanup();

	// Save out default file directories
	FEditorDirectories::Get().SaveLastDirectories();

	// Allow the game thread to finish processing any latent tasks.
	// Some editor functions may queue tasks that need to be run before the editor is finished.
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

	// Cleanup the misc editor
	FUnrealEdMisc::Get().OnExit();

	if( GLogConsole )
	{
		GLogConsole->Show( false );
	}

	delete GDebugToolExec;
	GDebugToolExec = NULL;
}

IMPLEMENT_MODULE( FDefaultModuleImpl, UnrealEd );
