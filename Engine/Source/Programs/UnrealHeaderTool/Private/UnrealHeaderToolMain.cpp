// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "UnrealHeaderTool.h"

#include "RequiredProgramMainCPPInclude.h"

IMPLEMENT_APPLICATION(UnrealHeaderTool, "UnrealHeaderTool");

DEFINE_LOG_CATEGORY(LogCompile);

bool GUHTWarningLogged = false;

/**
 * Application entry point
 *
 * @param	ArgC	Command-line argument count
 * @param	ArgV	Argument strings
 */
INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FString CmdLine;

	for (int32 Arg = 0; Arg < ArgC; Arg++)
	{
		FString LocalArg = ArgV[Arg];
		if (LocalArg.Contains(TEXT(" ")))
		{
			CmdLine += TEXT("\"");
			CmdLine += LocalArg;
			CmdLine += TEXT("\"");
		}
		else
		{
			CmdLine += LocalArg;
		}

		if (Arg + 1 < ArgC)
		{
			CmdLine += TEXT(" ");
		}
	}

	FString ShortCmdLine = FCommandLine::RemoveExeName(*CmdLine);
	ShortCmdLine = ShortCmdLine.Trim();	

	// Get game name from the commandline. It will later be used to load the correct ini files.
	FString ModuleInfoFilename;
	if (ShortCmdLine.Len() && **ShortCmdLine != TEXT('-'))
	{
		const TCHAR* CmdLinePtr = *ShortCmdLine;

		// Parse the game name or project filename.  UHT reads the list of plugins from there in case one of the plugins is UHT plugin.
		FString GameName = FParse::Token(CmdLinePtr, false);

		// This parameter is the absolute path to the file which contains information about the modules
		// that UHT needs to generate code for.
		ModuleInfoFilename = FParse::Token(CmdLinePtr, false );
	}

	GIsUCCMakeStandaloneHeaderGenerator = true;
	if (GEngineLoop.PreInit(*ShortCmdLine) != 0)
	{
		UE_LOG(LogCompile, Error, TEXT("Failed to initialize the engine (PreInit failed)."));
		return ECompilationResult::CrashOrAssert;
	}

	// Log full command line for UHT as UBT overrides LogInit verbosity settings
	UE_LOG(LogCompile, Log, TEXT("UHT Command Line: %s"), *CmdLine);

	if (ModuleInfoFilename.IsEmpty())
	{
		if (!FPlatformMisc::IsDebuggerPresent())
		{
			UE_LOG(LogCompile, Error, TEXT( "Missing module info filename on command line" ));
			return ECompilationResult::OtherCompilationError;
		}

		// If we have a debugger, let's use a pre-existing manifest file to streamline debugging
		// without the user having to faff around trying to get a UBT-generated manifest
		ModuleInfoFilename = FPaths::ConvertRelativePathToFull(FPlatformProcess::BaseDir(), TEXT("../../Source/Programs/UnrealHeaderTool/Resources/UHTDebugging.manifest"));
	}

	extern ECompilationResult::Type UnrealHeaderTool_Main(const FString& ModuleInfoFilename);
	ECompilationResult::Type Result = UnrealHeaderTool_Main(ModuleInfoFilename);

	FEngineLoop::AppPreExit();
	FEngineLoop::AppExit();

	if (Result == ECompilationResult::Succeeded && GUHTWarningLogged && GWarn->TreatWarningsAsErrors)
	{
		Result = ECompilationResult::OtherCompilationError;
	}

	return Result;
}

