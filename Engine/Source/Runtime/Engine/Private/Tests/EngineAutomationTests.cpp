// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "Tests/AutomationTestSettings.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#endif

#include "AutomationCommon.h"
#include "AutomationTestCommon.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"


namespace
{
	UWorld* GetSimpleEngineAutomationTestGameWorld(const int32 TestFlags)
	{
		// Accessing the game world is only valid for game-only 
		check((TestFlags & EAutomationTestFlags::ATF_ApplicationMask) == EAutomationTestFlags::ATF_Game);
		check(GEngine->GetWorldContexts().Num() == 1);
		check(GEngine->GetWorldContexts()[0].WorldType == EWorldType::Game);

		return GEngine->GetWorldContexts()[0].World();
	}

	/**
	* Populates the test names and commands for complex tests that are ran on all available maps
	*
	* @param OutBeautifiedNames - The list of map names
	* @param OutTestCommands - The list of commands for each test (The file names in this case)
	*/
	void PopulateTestsForAllAvailableMaps(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands)
	{
		TArray<FString> FileList;
#if WITH_EDITOR
		FEditorFileUtils::FindAllPackageFiles(FileList);
#else
		// Look directly on disk. Very slow!
		FPackageName::FindPackagesInDirectory(FileList, *FPaths::GameContentDir());
#endif

		// Iterate over all files, adding the ones with the map extension..
		for (int32 FileIndex = 0; FileIndex < FileList.Num(); FileIndex++)
		{
			const FString& Filename = FileList[FileIndex];

			// Disregard filenames that don't have the map extension if we're in MAPSONLY mode.
			if (FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension())
			{
				if (FAutomationTestFramework::GetInstance().ShouldTestContent(Filename))
				{
					OutBeautifiedNames.Add(FPaths::GetBaseFilename(Filename));
					OutTestCommands.Add(Filename);
				}
			}
		}
	}
}

/**
 * SetRes Verification - Verify changing resolution works
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST( FSetResTest, "System.Windows.Set Resolution", EAutomationTestFlags::ATF_Game )

/** 
 * Change resolutions, wait, and change back
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FSetResTest::RunTest(const FString& Parameters)
{
	//Gets the default map that the game uses.
	const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
	const FString& MapName = GameMapsSettings->GetGameDefaultMap();

	//Opens the actual default map in game.
	GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));

	//Gets the current resolution.
	int32 ResX = GSystemResolution.ResX;
	int32 ResY = GSystemResolution.ResY;
	FString RestoreResolutionString = FString::Printf(TEXT("setres %dx%d"), ResX, ResY);

	//Change the resolution and then restore it.
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("setres 640x480")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(RestoreResolutionString));

	return true;
}

/**
 * Stats verification - Toggle various "stats" commands
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST( FStatsVerificationMapTest, "System.Maps.Stats Verification", EAutomationTestFlags::ATF_Game )

/** 
 * Execute the loading of one map to verify screen captures and performance captures work
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FStatsVerificationMapTest::RunTest(const FString& Parameters)
{
	UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
	check(AutomationTestSettings);
	FString MapName = AutomationTestSettings->AutomationTestmap.FilePath;

	//Get the location of the map being used.
	FString Filename = FPaths::ConvertRelativePathToFull(MapName);

	//If the filename doesn't exist then we'll make the filename path relative to the engine instead of the game and the retry.
	if (!FPaths::FileExists(Filename))
	{
		FString EngDirectory = FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir());

		Filename = EngDirectory / MapName;

		Filename.ReplaceInline(TEXT("/Content/../../"), TEXT("/"),ESearchCase::CaseSensitive);
	}

	if (FPaths::FileExists(Filename))
	{
		//If the map is located in the engine folder then we'll make the path relative to that.  Otherwise it will be relative to the game content folder.
		if (Filename.Contains(TEXT("Engine"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
		{
			//This will be false if the map is on a different drive.
			if (FPaths::MakePathRelativeTo(Filename, *FPaths::EngineContentDir()))
			{
				FString ShortName = FPaths::GetBaseFilename(Filename);
				FString PathName = FPaths::GetPath(Filename);
				MapName = FString::Printf(TEXT("/Engine/%s/%s.%s"), *PathName, *ShortName, *ShortName);

				UE_LOG(LogEngineAutomationTests, Log, TEXT("Opening '%s'"), *MapName);

				GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));
			}
			else
			{
				UE_LOG(LogEngineAutomationTests, Error, TEXT("Invalid asset path: %s."), *Filename);
			}
		}
		else
		{
			//This will be false if the map is on a different drive.
			if (FPaths::MakePathRelativeTo(Filename, *FPaths::GameContentDir()))
			{
				FString ShortName = FPaths::GetBaseFilename(Filename);
				FString PathName = FPaths::GetPath(Filename);
				MapName = FString::Printf(TEXT("/Game/%s/%s.%s"), *PathName, *ShortName, *ShortName);

				UE_LOG(LogEngineAutomationTests, Log, TEXT("Opening '%s'"), *MapName);

				GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));
			}
			else
			{
				UE_LOG(LogEngineAutomationTests, Error, TEXT("Invalid asset path: %s."), *MapName);
			}
		}
	}
	else
	{
		UE_LOG(LogEngineAutomationTests, Log, TEXT("Automation test map doesn't exist or is not set: %s.  \nUsing the currently loaded map."), *Filename);
	}

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat game")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat game")));

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat scenerendering")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat scenerendering")));

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat memory")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat memory")));

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat slate")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat slate")));

	return true;
}

/**
 * LoadAutomationMap
 * Verification automation test to make sure features of map loading work (load, screen capture, performance capture)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST( FPerformanceCaptureTest, "System.Maps.Performance Capture", EAutomationTestFlags::ATF_Game )

/** 
 * Execute the loading of one map to verify screen captures and performance captures work
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FPerformanceCaptureTest::RunTest(const FString& Parameters)
{
	UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
	check(AutomationTestSettings);
	FString MapName = AutomationTestSettings->AutomationTestmap.FilePath;

	GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));
	ADD_LATENT_AUTOMATION_COMMAND(FEnqueuePerformanceCaptureCommands());

	return true;
}

/**
 * Latent command to take a screenshot of the viewport
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTakeViewportScreenshotCommand, FString, ScreenshotFileName);

bool FTakeViewportScreenshotCommand::Update()
{
	const bool bShowUI = false;
	const bool bAddFilenameSuffix = false;
	FScreenshotRequest::RequestScreenshot( ScreenshotFileName, bShowUI, bAddFilenameSuffix );
	return true;
}

/**
 * LoadAllMapsInGame
 * Verification automation test to make sure loading all maps succeed without crashing AND does performance captures
 */
IMPLEMENT_COMPLEX_AUTOMATION_TEST( FLoadAllMapsInGameTest, "Project.Maps.Load All In Game", EAutomationTestFlags::ATF_Game )

/** 
 * Requests a enumeration of all maps to be loaded
 */
void FLoadAllMapsInGameTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	PopulateTestsForAllAvailableMaps(OutBeautifiedNames, OutTestCommands);
}

/** 
 * Execute the loading of each map and performance captures
 *
 * @param Parameters - Should specify which map name to load
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FLoadAllMapsInGameTest::RunTest(const FString& Parameters)
{
	FString MapName = Parameters;

	//Open the map
	GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));

	if( FAutomationTestFramework::GetInstance().IsScreenshotAllowed() )
	{
		//Generate the screen shot name and path
		FString ScreenshotFileName;
		const FString TestName = FString::Printf(TEXT("LoadAllMaps_Game/%s"), *FPaths::GetBaseFilename(MapName));
		AutomationCommon::GetScreenshotPath(TestName, ScreenshotFileName, true);

		//Give the map some time to load
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.5f));
		//Take the screen shot
		ADD_LATENT_AUTOMATION_COMMAND(FTakeViewportScreenshotCommand(ScreenshotFileName));
		//Give the screen shot a chance to capture the scene
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(0.5f));
	}

	//Kick off any Automation matinees that are in this map
	ADD_LATENT_AUTOMATION_COMMAND(FEnqueuePerformanceCaptureCommands());

	return true;
}

/**
 * SaveGameTest
 * Test makes sure a save game (without UI) saves and loads correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST( FSaveGameTest, "System.Engine.Game.Noninteractive Save", EAutomationTestFlags::ATF_Game )

/** 
 * Saves and loads a savegame file
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FSaveGameTest::RunTest(const FString& Parameters)
{
	// automation save name
	const TCHAR* SaveName = TEXT("AutomationSaveTest");
	uint32 SavedData = 99;

	// the blob we are going to write out
	TArray<uint8> Blob;
	FMemoryWriter WriteAr(Blob);
	WriteAr << SavedData;

	// get the platform's save system
	ISaveGameSystem* Save = IPlatformFeaturesModule::Get().GetSaveGameSystem();

	// write it out
	if (Save->SaveGame(false, SaveName, 0, Blob) == false)
	{
		return false;
	}

	// make sure it was written
	if (Save->DoesSaveGameExist(SaveName, 0) == false)
	{
		return false;
	}

	// read it back in
	Blob.Empty();
	if (Save->LoadGame(false, SaveName, 0, Blob) == false)
	{
		return false;
	}

	// make sure it's the same data
	FMemoryReader ReadAr(Blob);
	uint32 LoadedData;
	ReadAr << LoadedData;

	// try to delete it (not all platforms can)
	if (Save->DeleteGame(false, SaveName, 0))
	{
		// make sure it's no longer there
		if (Save->DoesSaveGameExist(SaveName, 0) == true)
		{
			return false;
		}
	}

	return LoadedData == SavedData;
}

/**
 * Automation test to load a map and capture FPS performance charts
 */
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCinematicFPSPerfTest, "Project.Maps.Cinematic FPS Perf Capture", (EAutomationTestFlags::ATF_Game | EAutomationTestFlags::ATF_NonNullRHI));

void FCinematicFPSPerfTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	PopulateTestsForAllAvailableMaps(OutBeautifiedNames, OutTestCommands);
}

bool FCinematicFPSPerfTest::RunTest(const FString& Parameters)
{
	//Map to use for this test.
	const FString MapName = Parameters;

	//The name of the matinee actor that will be triggered.
	FString MatineeActorName;

	//Check we are running from commandline
	const FString CommandLine(FCommandLine::Get());
	if (CommandLine.Contains(TEXT("AutomationTests")))
	{
		//Get the name of the matinee to be used.
		//If the game was not launched with the -MatineeName argument then this test will be ran based on time.
		if (FParse::Value(*CommandLine, TEXT("MatineeName="), MatineeActorName))
		{
			//Load map
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));
			ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand(MapName));
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

			//Start the matinee and perform the FPS Chart
			ADD_LATENT_AUTOMATION_COMMAND(FMatineePerformanceCaptureCommand(MatineeActorName));
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

			return true;
		}
		else
		{
			UE_LOG(LogEngineAutomationTests, Log, TEXT("The matinee name was not specified.  Run the game with -MatineeName=\"Name of the matinee actor\"."));

			//Get the name of the console event to trigger the cinematic
			FString CinematicEventCommand;
			if (!FParse::Value(*CommandLine, TEXT("CE="), CinematicEventCommand))
			{
				UE_LOG(LogEngineAutomationTests, Log, TEXT("A console event command was not specified. Defaults to CE START.  Run the game with -CE=\"Command\"."));
				CinematicEventCommand = TEXT("CE Start");
			}

			//Get the length of time the cinematic will run
			float RunTime;
			if (!FParse::Value(*CommandLine, TEXT("RunTime="), RunTime))
			{
				UE_LOG(LogEngineAutomationTests, Log, TEXT("A run time length in seconds was not specified. Defaults to 60 seconds. Run the game with -RunTime=###."));
				RunTime = 60.f;
			}

			//Load map
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));
			ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand(MapName));
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

			//Start the matinee and perform the FPS Chart
			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(CinematicEventCommand));
			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StartFPSChart")));
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(RunTime));
			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StopFPSChart")));
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

			return true;

		}
	}
	//If the user is running from the UFE then we'll use the default values.
	//@todo Give the end user a way to specify the values for this test.

	UE_LOG(LogEngineAutomationTests, Log, TEXT("Running the FPS chart performance capturing for 60 seconds while in '%s'.\nThe default CE command won't be used at this time."), *MapName);

	//Load map
	ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand(MapName));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

	//Start the matinee and perform the FPS Chart
	ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StartFPSChart")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(60.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StopFPSChart")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLogTypesTest, "System.Automation Framework.Logging Test", EAutomationTestFlags::ATF_None)

bool FLogTypesTest::RunTest(const FString& Parameters)
{
	UE_LOG(LogEngineAutomationTests, Log, TEXT("This 'Log' log is created using the UE_LOG Macro"));
	UE_LOG(LogEngineAutomationTests, Display, TEXT("This 'Display' log is created using the UE_LOG Macro"));

	ExecutionInfo.LogItems.Add(TEXT("This was added to the ExecutionInfo.LogItems"));
	ExecutionInfo.Errors.Add(TEXT("This was added to the ExecutionInfo.Error."));
	if (ExecutionInfo.Errors.Last().Contains(TEXT("This was added to the ExecutionInfo.Error.")))
	{
		UE_LOG(LogEngineAutomationTests, Display, TEXT("ExecutionInfo.Error has the correct info when used."));
		ExecutionInfo.Errors.Empty();
	}

	ExecutionInfo.Warnings.Add(TEXT("This was added to the ExecutionInfo.Warnings"));
	if (ExecutionInfo.Warnings.Last().Contains(TEXT("This was added to the ExecutionInfo.Warnings")))
	{
		UE_LOG(LogEngineAutomationTests, Display, TEXT("ExecutionInfo.Warnings has the correct info when used."));
		ExecutionInfo.Warnings.Empty();
	}

	return true;
}

/* UAutomationTestSettings interface
 *****************************************************************************/

UAutomationTestSettings::UAutomationTestSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
}
