// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PrivatePch.h"
#include "RequiredProgramMainCPPInclude.h"

#include "BuildPatchTool.h"
#include "ToolMode.h"

using namespace BuildPatchTool;

IMPLEMENT_APPLICATION(BuildPatchTool, "BuildPatchTool");
DEFINE_LOG_CATEGORY(LogBuildPatchTool);

class FBuildPatchOutputDevice : public FOutputDevice
{
public:
	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override
	{
#if PLATFORM_WINDOWS
#if PLATFORM_USE_LS_SPEC_FOR_WIDECHAR
		printf( "\n%ls", *FOutputDeviceHelper::FormatLogLine( Verbosity, Category, V, GPrintLogTimes ) );
#else
		wprintf( TEXT( "\n%s" ), *FOutputDeviceHelper::FormatLogLine( Verbosity, Category, V, GPrintLogTimes ) );
#endif
		fflush( stdout );
#endif
	}
};

const TCHAR* HandleLegacyCommandline(const TCHAR* CommandLine)
{
	static FString CommandLineString;
	CommandLineString = CommandLine;

#if UE_BUILD_DEBUG
	// Run smoke tests in debug
	CommandLineString += TEXT(" -bForceSmokeTests ");
#endif

	// No longer supported options
	if (CommandLineString.Contains(TEXT("-nochunks")))
	{
		UE_LOG(LogBuildPatchTool, Error, TEXT("NoChunks is no longer a supported mode. Remove this commandline option."));
		return nullptr;
	}

	// Check for legacy tool mode switching, if we don't have a mode and this was not a -help request, add the correct mode
	if (!CommandLineString.Contains(TEXT("-mode=")) && !CommandLineString.Contains(TEXT("-help")))
	{
		if (CommandLineString.Contains(TEXT("-compactify")))
		{
			CommandLineString = CommandLineString.Replace(TEXT("-compactify"), TEXT("-mode=compactify"));
		}
		else if (CommandLineString.Contains(TEXT("-dataenumerate")))
		{
			CommandLineString = CommandLineString.Replace(TEXT("-dataenumerate"), TEXT("-mode=enumeration"));
		}
		// Patch generation did not have a mode flag, but does have some unique and required params
		else if (CommandLineString.Contains(TEXT("-BuildRoot=")) && CommandLineString.Contains(TEXT("-BuildVersion=")))
		{
			FString NewCommandline(TEXT("-mode=patchgeneration "), CommandLineString.Len());
			NewCommandline += CommandLineString;
			CommandLineString = MoveTemp(NewCommandline);
		}
	}

	return *CommandLineString;
}

EReturnCode RunBuildPatchTool()
{
	// Load the BuildPatchServices Module
	TSharedRef<IBuildPatchServicesModule> BuildPatchServicesModule = StaticCastSharedPtr<IBuildPatchServicesModule>(FModuleManager::Get().LoadModule(TEXT("BuildPatchServices"))).ToSharedRef();

	// Initialise the UObject system and process our uobject classes
	FModuleManager::Get().LoadModule(TEXT("CoreUObject"));
	FCoreDelegates::OnInit.Broadcast();
	ProcessNewlyLoadedUObjects();

	TSharedRef<IToolMode> ToolMode = FToolModeFactory::Create(BuildPatchServicesModule);
	return ToolMode->Execute();
}

EReturnCode BuildPatchToolMain(const TCHAR* CommandLine)
{
	// Add log device for stdout
	GLog->AddOutputDevice(new FBuildPatchOutputDevice());

	// Handle legacy commandlines
	CommandLine = HandleLegacyCommandline(CommandLine);
	if (CommandLine == nullptr)
	{
		return EReturnCode::ArgumentProcessingError;
	}

	// Initialise application
	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogBuildPatchTool, Log, TEXT("Executed with commandline: %s"), CommandLine);

	// Run the application
	EReturnCode ReturnCode = RunBuildPatchTool();

	// Shutdown
	FCoreDelegates::OnExit.Broadcast();

	return ReturnCode;
}

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FString CommandLine = TEXT("-usehyperthreading");
	for (int32 Option = 1; Option < ArgC; Option++)
	{
		CommandLine += TEXT(" ");
		FString Argument(ArgV[Option]);
		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;
				Argument.Split(TEXT("="), &ArgName, &ArgValue);
				Argument = FString::Printf(TEXT("%s=\"%s\""), *ArgName, *ArgValue);
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}
		CommandLine += Argument;
	}

	return static_cast<int32>(BuildPatchToolMain(*CommandLine));
}
