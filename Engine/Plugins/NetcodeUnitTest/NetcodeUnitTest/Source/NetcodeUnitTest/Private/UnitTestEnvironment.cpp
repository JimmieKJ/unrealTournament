// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "NUTUtil.h"
#include "UnitTestEnvironment.h"

#include "ClientUnitTest.h"

TMap<FString, FUnitTestEnvironment*> UnitTestEnvironments;


/**
 * FUnitTestEnvironment
 */

void FUnitTestEnvironment::AddUnitTestEnvironment(FString Game, FUnitTestEnvironment* Env)
{
	if (UnitTestEnvironments.Find(Game) == NULL)
	{
		UnitTestEnvironments.Add(Game, Env);

		if (Game == FApp::GetGameName())
		{
			UUnitTest::UnitEnv = Env;
		}
	}
}

FString FUnitTestEnvironment::GetDefaultMap(EUnitTestFlags UnitTestFlags)
{
	return TEXT("");
}

FString FUnitTestEnvironment::GetDefaultServerParameters()
{
	FString ReturnVal = TEXT("");
	FString CmdLineServerParms;
	FString FullLogCmds = TEXT("");

	if (NUTUtil::ParseValue(FCommandLine::Get(), TEXT("UnitTestServerParms="), CmdLineServerParms))
	{
		// LogCmds needs to be specially merged, since it may be specified in multiple places
		FString LogCmds;

		if (FParse::Value(*CmdLineServerParms, TEXT("-LogCmds="), LogCmds, false))
		{
			if (FullLogCmds.Len() > 0)
			{
				FullLogCmds += TEXT(",");
			}

			FullLogCmds += LogCmds.TrimQuotes();

			// Now remove the original LogCmds entry
			FString SearchStr = FString::Printf(TEXT("-LogCmds=\"%s\""), *LogCmds.TrimQuotes());

			CmdLineServerParms = CmdLineServerParms.Replace(*SearchStr, TEXT(""), ESearchCase::IgnoreCase);
		}


		ReturnVal += TEXT(" ");
		ReturnVal += CmdLineServerParms;
	}

	SetupDefaultServerParameters(ReturnVal, FullLogCmds);

	if (FullLogCmds.Len() > 0)
	{
		ReturnVal += FString::Printf(TEXT(" -LogCmds=\"%s\""), *FullLogCmds);
	}

	return ReturnVal;
}

FString FUnitTestEnvironment::GetDefaultClientParameters()
{
	FString ReturnVal = TEXT("");
	FString CmdLineClientParms;

	ReturnVal = TEXT("-nullrhi -windowed -resx=640 -resy=480");

	// @todo JohnB: Add merging of LogCmds, from above function, if adding default LogCmds for any game

	if (NUTUtil::ParseValue(FCommandLine::Get(), TEXT("UnitTestClientParms="), CmdLineClientParms))
	{
		ReturnVal += TEXT(" ");
		ReturnVal += CmdLineClientParms;
	}

	SetupDefaultClientParameters(ReturnVal);

	return ReturnVal;
}

FString FUnitTestEnvironment::GetDefaultClientConnectURL()
{
	return TEXT("");
}

void FUnitTestEnvironment::GetServerProgressLogs(const TArray<FString>*& OutStartProgressLogs, const TArray<FString>*& OutReadyLogs,
													const TArray<FString>*& OutTimeoutResetLogs)
{
	static TArray<FString> StartProgressLogs;
	static TArray<FString> ReadyLogs;
	static TArray<FString> TimeoutResetLogs;
	static bool bSetupLogs = false;

	if (!bSetupLogs)
	{
		// Start progress logs common to all games
		StartProgressLogs.Add(TEXT("LogLoad: LoadMap: "));
		StartProgressLogs.Add(TEXT("LogUnitTest: NUTActor not present in RuntimeServerActors - adding this"));
		StartProgressLogs.Add(TEXT("LogNet: Spawning: /Script/NetcodeUnitTest.NUTActor"));

		// Logs which indicate that the server is ready
		ReadyLogs.Add(TEXT("LogWorld: Bringing up level for play took: "));

		// Logs which should trigger a timeout reset for all games
		TimeoutResetLogs.Add(TEXT("LogStaticMesh: Building static mesh "));

		InitializeServerProgressLogs(StartProgressLogs, ReadyLogs, TimeoutResetLogs);

		bSetupLogs = true;
	}

	OutStartProgressLogs = &StartProgressLogs;
	OutReadyLogs = &ReadyLogs;
	OutTimeoutResetLogs = &TimeoutResetLogs;
}

