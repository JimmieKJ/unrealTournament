// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealFrontendPrivatePCH.h"
#include "UnrealFrontendMain.h"


static FString GSavedCommandLine;


int main( int argc, char *argv[] )
{
	for (int32 Option = 1; Option < argc; ++Option)
	{
		GSavedCommandLine += TEXT(" ");
		FString Argument(ANSI_TO_TCHAR(argv[Option]));

		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;

				Argument.Split( TEXT("="), &ArgName, &ArgValue );
				Argument = FString::Printf( TEXT("%s=\"%s\""), *ArgName, *ArgValue );
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}

		GSavedCommandLine += Argument;
	}
	
	FPlatformMisc::SetGracefulTerminationHandler();

#if !UE_BUILD_SHIPPING
	if (FParse::Param(*GSavedCommandLine,TEXT("crashreports")))
	{
		GAlwaysReportCrash = true;
	}
#endif

	GUseCrashReportClient = true;
	
#if UE_BUILD_DEBUG
	if (!GAlwaysReportCrash)
#else
	if (FPlatformMisc::IsDebuggerPresent() && !GAlwaysReportCrash)
#endif
	{
		UnrealFrontendMain(*GSavedCommandLine);
	}
	else
	{
		GIsGuarded = 1;
		UnrealFrontendMain(*GSavedCommandLine);
		GIsGuarded = 0;
	}

	FEngineLoop::AppExit();
	
	return 0;
}
