// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealFrontendPrivatePCH.h"
#include "UnrealFrontendMain.h"
#include "RequiredProgramMainCPPInclude.h"

#include "DeployCommand.h"
#include "LaunchCommand.h"
#include "PackageCommand.h"
#include "StatsConvertCommand.h"
#include "StatsDumpMemoryCommand.h"
#include "UserInterfaceCommand.h"


IMPLEMENT_APPLICATION(UnrealFrontend, "UnrealFrontend");


/**
 * Platform agnostic implementation of the main entry point.
 */
int32 UnrealFrontendMain( const TCHAR* CommandLine )
{
	FCommandLine::Set(CommandLine);

	FString Command;
	FString Params;
	FString NewCommandLine = CommandLine;

	// process command line parameters
	bool bRunCommand = FParse::Value(*NewCommandLine, TEXT("-RUN="), Command);
	
	if (bRunCommand)
	{
		// extract off any -PARAM= parameters so that they aren't accidentally parsed by engine init
		FParse::Value(*NewCommandLine, TEXT("-PARAMS="), Params);

		if (Params.Len() > 0)
		{
			// remove from the command line & trim quotes
			NewCommandLine = NewCommandLine.Replace(*Params, TEXT(""));
			Params = Params.TrimQuotes();
		}
	}

	if (!FParse::Param(*NewCommandLine, TEXT("-Messaging")))
	{
		NewCommandLine += TEXT(" -Messaging");
	}

	// initialize core
	GEngineLoop.PreInit(*NewCommandLine);
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	bool Succeeded = true;

	// execute desired command
	if (bRunCommand)
	{
		if (Command.Equals(TEXT("PACKAGE"), ESearchCase::IgnoreCase))
		{
			FPackageCommand::Run();
		}
		else if (Command.Equals(TEXT("DEPLOY"), ESearchCase::IgnoreCase))
		{
			Succeeded = FDeployCommand::Run();
		}
		else if (Command.Equals(TEXT("LAUNCH"), ESearchCase::IgnoreCase))
		{
			Succeeded = FLaunchCommand::Run(Params);
		}
		else if (Command.Equals(TEXT("CONVERT"), ESearchCase::IgnoreCase))
		{
			FStatsConvertCommand::Run();
		}
		else if( Command.Equals( TEXT("MEMORYDUMP"), ESearchCase::IgnoreCase ) )
		{
			FStatsMemoryDumpCommand::Run();
		}
	}
	else
	{
		FUserInterfaceCommand::Run();
	}

	// shut down
	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();

#if STATS
	FThreadStats::StopThread();
#endif

	FTaskGraphInterface::Shutdown();

	return Succeeded ? 0 : -1;
}
