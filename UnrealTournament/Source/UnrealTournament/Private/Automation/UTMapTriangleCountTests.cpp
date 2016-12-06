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

// Facing Worlds test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCtfPistolaTriangleCountTest, "UTPerf.TriangleCount.Maps.Ctf-Pistola", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter);

// Chill test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDmChillTriangleCountTest, "UTPerf.TriangleCount.Maps.DM-Chill", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter);

// Outpost23 test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDmOutpostTriangleCountTest, "UTPerf.TriangleCount.Maps.DM-Outpost23", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter);

// Underland test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDmUnderlandTriangleCountTest, "UTPerf.TriangleCount.Maps.DM-Underland", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter);

// Lea test
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDmLeaTriangleCountTest, "UTPerf.TriangleCount.Maps.DM-Lea", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter);


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

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FProfileGPUCommand, MapBugItGoLocationData*, PerfData);

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
	PlayerController->ConsoleCommand(FString::Printf(TEXT("open %s?TimeLimit=0"), *MapName));
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

	FScreenshotRequest::RequestScreenshot(FileName, false, true);
	return true;
}

bool FProfileGPUCommand::Update()
{
	AUTPlayerController* PlayerController = GetPlayerController();
	if (PlayerController)
	{
		PlayerController->ConsoleCommand(TEXT("ProfileGPU"));
	}

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

void SetUpBugItGoArrayForPistola(TArray<MapBugItGoLocationData*>& BugItGoLocs)
{

	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2770.116211 -9271.802734 760.150024 10.604571 45.737614 0.000000"), TEXT("B1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("2031.003296 -6976.490234 610.291382 2.212967 89.631310 0.000000"), TEXT("B2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("2511.114258 -4049.001953 610.512451 -3.965526 -134.723419 0.000000"), TEXT("B3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2476.147705 -6560.151855 610.291382 -4.149886 90.276695 0.000000"), TEXT("B4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2830.336182 -4152.070801 535.436646 2.028450 -44.722984 0.000000"), TEXT("B5")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-559.337769 -4513.270996 605.291382 3.319431 5.348769 0.000000"), TEXT("B6")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("49.114491 -6483.455566 1360.291382 -10.881392 46.107506 0.000000"), TEXT("B7")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-5922.649902 -1050.193848 84.525017 6.915129 -44.630409 -0.000000"), TEXT("M1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-4787.927734 -1250.936890 360.291382 4.057542 9.036789 0.000000"), TEXT("M2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2491.706055 -2633.815918 273.185028 3.135405 55.512081 0.000000"), TEXT("M3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("90.620514 -2733.057861 610.291443 9.866993 90.091919 0.000000"), TEXT("M4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-795.163513 990.680908 749.871948 -4.333502 -112.869286 0.000000"), TEXT("M5")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-11.652017 2740.948242 610.291382 9.037357 -90.000587 0.000000"), TEXT("M6")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("793.048462 -1102.029297 705.332642 -1.106183 60.122314 0.000000"), TEXT("M7")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("2991.883301 2854.829102 281.068146 -6.085398 -127.255089 0.000000"), TEXT("M8")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("5220.641602 1699.468506 150.291397 11.711724 134.261017 0.000000"), TEXT("M9")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("4538.097656 1229.895386 380.581421 3.320346 -160.083313 0.000000"), TEXT("M10")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("2782.506104 9278.123047 760.441223 6.824861 -135.000885 -0.000000"), TEXT("R1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2031.438965 6893.375000 610.291443 3.689497 -71.097031 0.000000"), TEXT("R2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2431.436035 4036.995605 605.291382 7.194142 44.906403 0.000000"), TEXT("R3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("424.429871 4494.573242 610.291382 7.931823 169.762711 0.000000"), TEXT("R4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("3140.395020 3949.074463 535.328430 5.165549 140.807281 0.000000"), TEXT("R5")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("2473.687256 6663.465820 610.291382 -2.488019 -86.313805 0.000000"), TEXT("R6")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-10.816180 6466.688477 1360.291382 -9.680611 -135.463333 0.000000"), TEXT("R7")));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		BugItGoLocs[i]->MapName = TEXT("CTF-Pistola");
	}
}

void SetUpBugItGoArrayForChill(TArray<MapBugItGoLocationData*>& BugItGoLocs)
{

	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-4569.014648 2929.963867 610.150024 7.134269 -22.334402 0.000000"), TEXT("1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2454.542480 4.532268 360.291382 8.278592 -0.120730 0.000000"), TEXT("2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-29.896004 -3791.466797 360.291382 6.428840 88.328766 0.000000"), TEXT("3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("3196.702393 -3432.716309 610.291382 2.376673 139.874954 0.000000"), TEXT("4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("4939.889160 -1179.935303 610.149963 18.670937 138.026001 0.000000"), TEXT("5")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("3500.304199 3512.795654 1220.291382 -0.618863 -134.229218 0.000000"), TEXT("6")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("184.689240 4664.689453 360.149994 11.360161 -102.222069 0.000000"), TEXT("7")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-1256.190674 -20.697317 133.328278 20.520504 1.007314 -0.000000"), TEXT("8")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("3.319853 -1255.844238 110.291336 23.339104 88.911125 0.000000"), TEXT("9")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("1079.819092 55.275425 110.291336 18.935122 -177.988312 0.000000"), TEXT("10")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("65.302826 1163.805176 110.291336 16.909260 -93.871735 0.000000"), TEXT("11")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2053.390381 -1230.621216 360.291351 11.007839 20.896585 0.000000"), TEXT("12")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-5109.096191 947.245361 610.150024 0.966663 -15.727427 -0.000000"), TEXT("13")));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		BugItGoLocs[i]->MapName = TEXT("DM-Chill");
	}
}

void SetUpBugItGoArrayForOutpost(TArray<MapBugItGoLocationData*>& BugItGoLocs)
{

	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("12934.916016 -8712.219727 111.162392 -2.906518 -158.944214 0.000000"), TEXT("1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("9396.947266 -8841.556641 -239.850037 14.357453 80.122101 0.000000"), TEXT("2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("6963.653320 -7489.480469 160.291397 -3.787469 -60.928452 0.000000"), TEXT("3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("6539.349121 -13483.480469 359.340485 10.217197 45.296120 0.000000"), TEXT("4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("13034.932617 -11759.462891 10.150293 2.680774 161.573395 0.000000"), TEXT("5")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("9098.356445 -10101.762695 -189.849930 4.354245 136.646774 0.000000"), TEXT("6")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("7564.976563 -5149.645508 160.150146 1.007309 -44.709934 0.000000"), TEXT("7")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("10104.456055 -7658.089844 810.151978 -17.136936 135.607162 0.000000"), TEXT("8")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("12320.502930 -5260.003906 510.150024 -7.625442 -135.415100 -0.000000"), TEXT("9")));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		BugItGoLocs[i]->MapName = TEXT("DM-Outpost23");
	}
}

void SetUpBugItGoArrayForUnderland(TArray<MapBugItGoLocationData*>& BugItGoLocs)
{

	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("3709.959961 1266.624390 -555.250732 0.000396 167.181839 -0.000000"), TEXT("1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("2662.057129 4260.273438 -1139.709106 11.065950 -128.913971 0.000000"), TEXT("2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-85.663918 2663.154053 -1138.148193 12.357430 101.802948 0.000000"), TEXT("3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-690.502014 3658.378418 -1130.780029 9.775482 -124.488235 0.000000"), TEXT("4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-3727.432617 3616.298340 -1139.849976 12.357450 -48.504509 0.000000"), TEXT("5")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-4716.317871 -426.306030 -1110.135620 10.513197 35.870144 0.000000"), TEXT("6")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-3040.423340 -3395.712891 -389.849884 8.576784 46.382679 0.000000"), TEXT("7")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("702.852966 -3303.029541 -638.595215 8.115699 78.564980 0.000000"), TEXT("8")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("1715.968750 -1903.320435 -637.717590 2.121880 -160.543762 0.000000"), TEXT("9")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("3147.981201 -823.677490 -611.197815 -1.751151 -164.601074 0.000000"), TEXT("10")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-28.570250 -276.392578 -389.708679 -4.056559 -89.447479 0.000000"), TEXT("11")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2.437406 341.059906 -389.708679 -10.050295 88.431358 0.000000"), TEXT("12")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2069.160645 2455.801270 -139.849548 2.490758 -74.048607 0.000000"), TEXT("13")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-2099.381592 2733.675049 610.292175 -12.539974 -63.258987 0.000000"), TEXT("14")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("48.348816 784.776917 1610.274780 -39.466412 96.730446 0.000000"), TEXT("15")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("15.586778 -809.061035 1610.150146 -52.284004 -88.617508 0.000000"), TEXT("16")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("-278.599213 5604.346191 -1134.286011 7.746841 -83.821953 0.000000"), TEXT("17")));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		BugItGoLocs[i]->MapName = TEXT("DM-Underland");
	}
}


void SetUpBugItGoArrayForLea(TArray<MapBugItGoLocationData*>& BugItGoLocs)
{

	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("3885.265625 10954.386719 176.550003 2.859008 -48.687981 -0.000000"), TEXT("1")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("6951.158691 10752.072266 384.550049 0.000369 -130.296677 0.000000"), TEXT("2")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("9066.656250 10559.362305 592.550049 -1.844144 -131.311020 0.000000"), TEXT("3")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("9877.040039 9239.397461 696.691406 -3.688056 -133.800919 0.000000"), TEXT("4")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("10223.346680 5726.966309 384.550018 -2.120302 137.674515 0.000000"), TEXT("5")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("4578.250488 5552.143555 488.550018 -2.949986 90.922562 -0.000000"), TEXT("6")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("4165.542969 6917.552246 488.550018 7.193480 -0.829376 0.000000"), TEXT("7")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("5831.369141 7689.363281 644.691406 -27.663269 50.256630 0.000000"), TEXT("8")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("8146.325684 7727.943359 748.691406 -7.653144 110.195068 0.000000"), TEXT("9")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("5142.500977 6435.148438 -343.307587 -0.276131 75.800034 -0.000000"), TEXT("10")));
	BugItGoLocs.Add(new MapBugItGoLocationData(TEXT("5784.738281 9302.557617 176.691452 -17.611969 -2.950178 -0.000000"), TEXT("11")));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		BugItGoLocs[i]->MapName = TEXT("DM-Lea");
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
		ADD_LATENT_AUTOMATION_COMMAND(FProfileGPUCommand(BugItGoLocs[i]));

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
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
		ADD_LATENT_AUTOMATION_COMMAND(FProfileGPUCommand(BugItGoLocs[i]));

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

bool FDmChillTriangleCountTest::RunTest(const FString& Parameters)
{
#if STATS
	// Load map
	TArray<MapBugItGoLocationData*> BugItGoLocs;
	SetUpBugItGoArrayForChill(BugItGoLocs);
	ADD_LATENT_AUTOMATION_COMMAND(FTravelToMapCommand(TEXT("DM-Chill")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerToBeReady);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FRunBugItGoCommand(BugItGoLocs[i]))
		
		// Switch to high
		ADD_LATENT_AUTOMATION_COMMAND(FChangeGraphicsQualityCommand(true));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
		ADD_LATENT_AUTOMATION_COMMAND(FProfileGPUCommand(BugItGoLocs[i]));

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


bool FDmOutpostTriangleCountTest::RunTest(const FString& Parameters)
{
#if STATS
	// Load map
	TArray<MapBugItGoLocationData*> BugItGoLocs;
	SetUpBugItGoArrayForOutpost(BugItGoLocs);
	ADD_LATENT_AUTOMATION_COMMAND(FTravelToMapCommand(TEXT("DM-Outpost23")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerToBeReady);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FRunBugItGoCommand(BugItGoLocs[i]))
		
		// Switch to high
		ADD_LATENT_AUTOMATION_COMMAND(FChangeGraphicsQualityCommand(true));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
		ADD_LATENT_AUTOMATION_COMMAND(FProfileGPUCommand(BugItGoLocs[i]));

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

bool FDmUnderlandTriangleCountTest::RunTest(const FString& Parameters)
{
#if STATS
	// Load map
	TArray<MapBugItGoLocationData*> BugItGoLocs;
	SetUpBugItGoArrayForUnderland(BugItGoLocs);
	ADD_LATENT_AUTOMATION_COMMAND(FTravelToMapCommand(TEXT("DM-Underland")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerToBeReady);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FRunBugItGoCommand(BugItGoLocs[i]))
		
		// Switch to high
		ADD_LATENT_AUTOMATION_COMMAND(FChangeGraphicsQualityCommand(true));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
		ADD_LATENT_AUTOMATION_COMMAND(FProfileGPUCommand(BugItGoLocs[i]));

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

bool FDmLeaTriangleCountTest::RunTest(const FString& Parameters)
{
#if STATS
	// Load map
	TArray<MapBugItGoLocationData*> BugItGoLocs;
	SetUpBugItGoArrayForLea(BugItGoLocs);
	ADD_LATENT_AUTOMATION_COMMAND(FTravelToMapCommand(TEXT("DM-Lea")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerToBeReady);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FRunBugItGoCommand(BugItGoLocs[i]))
		
		// Switch to high
		ADD_LATENT_AUTOMATION_COMMAND(FChangeGraphicsQualityCommand(true));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
		ADD_LATENT_AUTOMATION_COMMAND(FProfileGPUCommand(BugItGoLocs[i]));

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

bool FCtfPistolaTriangleCountTest::RunTest(const FString& Parameters)
{
#if STATS
	// Load map
	TArray<MapBugItGoLocationData*> BugItGoLocs;
	SetUpBugItGoArrayForPistola(BugItGoLocs);
	ADD_LATENT_AUTOMATION_COMMAND(FTravelToMapCommand(TEXT("Ctf-Pistola")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerToBeReady);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
	for (int i = 0; i < BugItGoLocs.Num(); i++)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FRunBugItGoCommand(BugItGoLocs[i]))
		
		// Switch to high
		ADD_LATENT_AUTOMATION_COMMAND(FChangeGraphicsQualityCommand(true));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
		ADD_LATENT_AUTOMATION_COMMAND(FProfileGPUCommand(BugItGoLocs[i]));

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