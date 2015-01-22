// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperPrivatePCH.h"
#include "Database.h"


#include "ISourceControlModule.h"

FCrashDebugHelperLinux::FCrashDebugHelperLinux()
{
}

FCrashDebugHelperLinux::~FCrashDebugHelperLinux()
{
}

bool FCrashDebugHelperLinux::ParseCrashDump(const FString& InCrashDumpName, FCrashDebugInfo& OutCrashDebugInfo)
{
	if (bInitialized == false)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("ParseCrashDump: CrashDebugHelper not initialized"));
		return false;
	}

	//if (ParseCrashDump_Linux(*InCrashDumpName, OutCrashDebugInfo) == true)
	{
		return true;
	}

	return false;
}

bool FCrashDebugHelperLinux::SyncAndDebugCrashDump(const FString& InCrashDumpName)
{
	if (bInitialized == false)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("SyncAndDebugCrashDump: CrashDebugHelper not initialized"));
		return false;
	}

	FCrashDebugInfo CrashDebugInfo;
	
	// Parse the crash dump if it hasn't already been...
	if (ParseCrashDump(InCrashDumpName, CrashDebugInfo) == false)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("SyncAndDebugCrashDump: Failed to parse crash dump %s"), *InCrashDumpName);
		return false;
	}

	return true;
}

bool FCrashDebugHelperLinux::CreateMinidumpDiagnosticReport( const FString& InCrashDumpName )
{
	return true;
}
