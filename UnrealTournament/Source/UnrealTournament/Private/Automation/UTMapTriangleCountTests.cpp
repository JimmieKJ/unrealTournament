// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "AutomationCommon.h"
#include "UTPlayerController.h"
#include "StatsData.h"
#include "PerfCountersModule.h"

// All the info we could ever hope to need for one of these tests since latents can only take one arg.
struct MapBugItGoLocationData
{
	FString BugItGoLocation;
	FString ScreenshotLabel;
	FString MapName;
	float LowQualityTriangles;
	float LowQualityFPS;
	FString LowQualityScreenshotLoc;
	float HighQualityTriangles;
	float HighQualityFPS;
	FString HighQualityScreenshotLoc;
	MapBugItGoLocationData(FString InLoc, FString InLabel)
	{
		BugItGoLocation = InLoc;
		ScreenshotLabel = InLabel;
		LowQualityTriangles = 0;
		HighQualityTriangles = 0;
	}
};

// TitanPass test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCtfTitanTriangleCountTest, "UTPerf.TriangleCount.Maps.Ctf-TitanPass", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter);

// Facing Worlds test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCtfFaceTriangleCountTest, "UTPerf.TriangleCount.Maps.Ctf-Face", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter);

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTravelToMapCommand, FString, MapName);

// Hold in place until we reach the Waiting to Start screen
DEFINE_LATENT_AUTOMATION_COMMAND(FWaitForPlayerToBeReady);

// Back out when we're done
DEFINE_LATENT_AUTOMATION_COMMAND(FReturnToMainMenuCommand);

// BugItGo to a location
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FRunBugItGoCommand, MapBugItGoLocationData*, CommandString);

// Toggle LOD distance
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FChangeGraphicsQualityCommand, bool, bHighQual);

// Record perf
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FRecordTriangleNumbersCommand, MapBugItGoLocationData*, PerfData);

// Spew results
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FOutputTestResultsCommand, TArray<MapBugItGoLocationData*>, PerfData);


UWorld* GetSimpleAutomationTestGameWorld(const int32 TestFlags)
{
	// Accessing the game world is only valid for game-only 
	check((TestFlags & EAutomationTestFlags::ApplicationContextMask) == EAutomationTestFlags::ClientContext);
	check(GEngine->GetWorldContexts().Num() == 1);
	check(GEngine->GetWorldContexts()[0].WorldType == EWorldType::Game);

	return GEngine->GetWorldContexts()[0].World();
}


UWorld* GetAnyValidWorld()
{
	UWorld* World = nullptr;
	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	// If theres only one world just use that (GetSimpleAutomationTestGameWorld will check is good to use !)
	if (WorldContexts.Num() == 1)
	{
		World = GetSimpleAutomationTestGameWorld(EAutomationTestFlags::ClientContext);
	}
	else
	{
		for (const FWorldContext& Context : WorldContexts)
		{
			if (Context.World() != NULL)
			{
				World = Context.World();
				break;
			}
		}
	}
	return World;
}

AUTPlayerController * GetPlayerController()
{
	UWorld * TheWorld = GetAnyValidWorld();
	AUTPlayerController* PlayerController = nullptr;
	if (TheWorld != nullptr)
	{
		PlayerController = Cast<AUTPlayerController>(TheWorld->GetFirstPlayerController());
	}
	return PlayerController;
} 

// Digs through our counters to find our Triangles Drawn value.
float GetNumTrianglesInScene()
{
#if STATS
	IPerfCounters* PerfCounters = IPerfCountersModule::Get().GetPerformanceCounters();
	
	if (FGameThreadHudData* ViewData = FHUDGroupGameThreadRenderer::Get().Latest)
	{
		for (const FHudGroup& HudGroup : ViewData->HudGroups)
		{
		
			for (int i = 0; i < HudGroup.CountersAggregate.Num(); i++)
			{
				const FName StatName = HudGroup.CountersAggregate[i].GetShortName();
				if (StatName.ToString().Contains(TEXT("Triangles")))
				{
					return HudGroup.CountersAggregate[i].GetValue_double(EComplexStatField::IncAve);
				}
			}
		}
	}
#endif
	
	return 0.f;
}

bool FReturnToMainMenuCommand::Update()
{
	AUTPlayerController* PlayerController = GetPlayerController();
	if (PlayerController)
	{
		PlayerController->ClientReturnToLobby();
	}
	return true;
}

bool FChangeGraphicsQualityCommand::Update()
{
	AUTPlayerController* PlayerController = GetPlayerController();
	if (bHighQual)
	{
		PlayerController->ConsoleCommand(TEXT("r.StaticMeshLODDistanceScale 1"));
	}
	else
	{
		PlayerController->ConsoleCommand(TEXT("r.StaticMeshLODDistanceScale 100"));
	}
	return true;
}

bool FTravelToMapCommand::Update()
{
	AUTPlayerController* PlayerController = GetPlayerController();
	PlayerController->ConsoleCommand(FString::Printf(TEXT("open %s"), *MapName));
	PlayerController->ConsoleCommand(TEXT("stat RHI"));
	return true;
}

bool FWaitForPlayerToBeReady::Update()
{
	AUTPlayerController* PlayerController = GetPlayerController();
	if (PlayerController && PlayerController->GetPawn())
	{
		return true;
	}
	if (PlayerController && PlayerController->bPlayerIsWaiting)
	{
		PlayerController->StartFire();
	}
	return false;
}


// Record data to object, and take the screenshot we care about.
bool FRecordTriangleNumbersCommand::Update()
{
	FString FileName = PerfData->ScreenshotLabel;
	if (!PerfData->HighQualityTriangles)
	{
		PerfData->HighQualityTriangles = GetNumTrianglesInScene();
		FileName = FString::Printf(TEXT("%s%d/%s/High/%s"), *FPaths::ScreenShotDir(), FEngineVersion::Current().GetChangelist(), *PerfData->MapName, *FileName);
		PerfData->HighQualityScreenshotLoc = FileName;
	}
	else
	{
		PerfData->LowQualityTriangles = GetNumTrianglesInScene();
		FileName = FString::Printf(TEXT("%s%d/%s/Low/%s"), *FPaths::ScreenShotDir(), FEngineVersion::Current().GetChangelist(), *PerfData->MapName, *FileName);
		PerfData->LowQualityScreenshotLoc = FileName;
	}
	AUTPlayerController* PlayerController = GetPlayerController();
	FScreenshotRequest::RequestScreenshot(FileName, false, true);
	return true;
}

// Write out our results CSC.
bool FOutputTestResultsCommand::Update()
{
	if (!PerfData.Num())
	{
		return true;
	}
	FArchive* ResultsArchive = IFileManager::Get().CreateFileWriter(*FString::Printf(TEXT("%s/Performance/%d/%s_TriangleCounts.csv"), *FPaths::AutomationDir(), FEngineVersion::Current().GetChangelist(), *PerfData[0]->MapName));
	FString LineToLog = TEXT("Loc Label, BugItGo Command, HQ Triangles, HQ Screenshot Loc, LQ Triangles, LQ Screenshot Loc\n");
	ResultsArchive->Serialize(TCHAR_TO_ANSI(*LineToLog), LineToLog.Len());

	for (int i = 0; i < PerfData.Num(); i++)
	{
		LineToLog = FString::Printf(TEXT("%s, %s, %0.3f, %s, %0.3f, %s \n"), *PerfData[i]->ScreenshotLabel, *PerfData[i]->BugItGoLocation, PerfData[i]->HighQualityTriangles, *PerfData[i]->HighQualityScreenshotLoc, PerfData[i]->LowQualityTriangles, *PerfData[i]->LowQualityScreenshotLoc);
		ResultsArchive->Serialize(TCHAR_TO_ANSI(*LineToLog), LineToLog.Len());
	}

	AUTPlayerController* PlayerController = GetPlayerController();
	PlayerController->ConsoleCommand(TEXT("stat RHI"));
	ResultsArchive->Close();
	return true;
}




bool FRunBugItGoCommand::Update()
{
	AUTPlayerController* PlayerController = GetPlayerController();
	PlayerController->ConsoleCommand(FString::Printf(TEXT("BugItGo %s"), *CommandString->BugItGoLocation));
	return true;
}



void SetUpBugItGoArrayForCtfTitan(TArray<MapBugItGoLocationData*>& BugItGoLocs)
{

	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-132.634781 12929.084961 1360.291382 -10.107505 -82.963509 0.000000"), TEXT("B1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("4692.007324 11523.694336 360.291412 -2.688417 -133.614624 0.000000"), TEXT("B2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-1855.242554 9896.162109 360.149994 7.096478 -9.101213 0.000000"), TEXT("B3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-34.520039 8160.922363 -629.128906 3.118111 -84.045937 0.00"), TEXT("B4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("1506.717285 10272.153320 1360.291382 -8.571587 -86.153191 0.000000"), TEXT("B5")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-3553.693115 8516.228516 -301.275085 9.247119 -41.210941 0.000000"), TEXT("M1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("2982.323975 6925.134277 -1139.706299 10.214752 -140.026138 0.000000"), TEXT("M2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("2980.956299 3757.028809 -1143.865723 15.053409 130.405670 0.000000"), TEXT("M3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("3137.741455 6805.257813 -1139.847656 11.720140 -138.305771 0.000000"), TEXT("M4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-566.856873 5984.652344 -1626.218384 16.773783 -125.080292 0.000000"), TEXT("M5")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("159.696838 3697.018555 -377.208588 12.902780 90.392334 0.000000"), TEXT("M6")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-14.864383 6505.677246 -377.208588 11.720030 -84.127373 0.000000"), TEXT("M7")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("3053.947754 -2863.415771 1360.150024 10.860007 -156.904510 0.000000"), TEXT("R1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2.516719 -5059.051758 1610.150024 -5.376240 93.742813 0.000000"), TEXT("R2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("24.065208 -2830.706543 1360.291382 -8.279364 93.742813 0.000000"), TEXT("R3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-4886.203125 -2012.046265 360.149994 -5.023057 63.098171 0.000000"), TEXT("R4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-3317.424561 -74.183365 114.331306 -6.528377 64.818573 0.000000"), TEXT("R5")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-36.068531 -670.667969 335.291382 -15.345351 93.957756 0.000000"), TEXT("R6")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("1865.300781 501.489838 360.149994 19.170113 -145.614441 0.000000"), TEXT("R7")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-1455.244263 -228.753510 1366.840698 -9.216602 95.678215 0.000000"), TEXT("R8")));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		BugItGoLocs[i]->MapName = TEXT("CTF-TitanPass");
	}
}

void SetUpBugItGoArrayForFacingWorlds(TArray<MapBugItGoLocationData*>& BugItGoLocs)
{

	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-7679.899414 43.093189 145.150024 16.414152 1.845065 0.000000"), TEXT("B1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-8984.934570 86.133652 145.149994 15.215544 -2.397206 0.000000"), TEXT("B2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-5196.957031 33.230335 109.766861 11.342683 179.261688 0.000000"), TEXT("B3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-7802.920410 2993.942627 128.800125 8.484085 -16.137541 0.000000"), TEXT("B4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-7794.481445 -2745.491943 127.641068 11.803208 14.385836 0.000000"), TEXT("B5")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("165.781464 -1286.321411 1477.807861 2.121505 172.255219 0.000000"), TEXT("M1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-109.116364 805.394409 1456.353394 4.334308 -3.134401 -0.000000"), TEXT("M2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("8000.000000 2777.649414 120.904823 8.115630 -169.025696 0.000000"), TEXT("R1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("8057.553223 -2725.358887 124.304947 11.158609 166.629990 0.000000"), TEXT("R2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("8999.993164 162.561340 145.150009 9.590419 -175.203934 0.000000"), TEXT("R3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("7664.160156 126.364815 145.236145 10.973640 -174.465958 0.000000"), TEXT("R4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("4941.095215 16.626684 105.947304 11.711404 0.462510 0.000000"), TEXT("R5")));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		BugItGoLocs[i]->MapName = TEXT("CTF-Face");
	}
}

bool FCtfTitanTriangleCountTest::RunTest(const FString& Parameters)
{
#if STATS
	// Load map
	TArray<MapBugItGoLocationData*> BugItGoLocs;
	SetUpBugItGoArrayForCtfTitan(BugItGoLocs);
	ADD_LATENT_AUTOMATION_COMMAND(FTravelToMapCommand(TEXT("CTF-TitanPass")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerToBeReady);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FRunBugItGoCommand(BugItGoLocs[i]))
			// Switch to high
			ADD_LATENT_AUTOMATION_COMMAND(FChangeGraphicsQualityCommand(true));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
		ADD_LATENT_AUTOMATION_COMMAND(FRecordTriangleNumbersCommand(BugItGoLocs[i]));
		// Switch to low
		ADD_LATENT_AUTOMATION_COMMAND(FChangeGraphicsQualityCommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
		ADD_LATENT_AUTOMATION_COMMAND(FRecordTriangleNumbersCommand(BugItGoLocs[i]));
	}

	ADD_LATENT_AUTOMATION_COMMAND(FOutputTestResultsCommand(BugItGoLocs));
	
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));

	ADD_LATENT_AUTOMATION_COMMAND(FReturnToMainMenuCommand);

	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(30.f));
#endif
	return true;
}

bool FCtfFaceTriangleCountTest::RunTest(const FString& Parameters)
{
#if STATS
	// Load map
	TArray<MapBugItGoLocationData*> BugItGoLocs;
	SetUpBugItGoArrayForFacingWorlds(BugItGoLocs);
	ADD_LATENT_AUTOMATION_COMMAND(FTravelToMapCommand(TEXT("CTF-Face")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerToBeReady);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FRunBugItGoCommand(BugItGoLocs[i]))
			// Switch to high
			ADD_LATENT_AUTOMATION_COMMAND(FChangeGraphicsQualityCommand(true));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));;
		ADD_LATENT_AUTOMATION_COMMAND(FRecordTriangleNumbersCommand(BugItGoLocs[i]));
		// Switch to low
		ADD_LATENT_AUTOMATION_COMMAND(FChangeGraphicsQualityCommand(false));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
		ADD_LATENT_AUTOMATION_COMMAND(FRecordTriangleNumbersCommand(BugItGoLocs[i]));
	}

	ADD_LATENT_AUTOMATION_COMMAND(FOutputTestResultsCommand(BugItGoLocs));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));

	ADD_LATENT_AUTOMATION_COMMAND(FReturnToMainMenuCommand);

	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(30.f));
#endif
	return true;
}