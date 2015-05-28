// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperPrivatePCH.h"
#include "CrashDebugHelperLinux.h"

FCrashDebugHelperLinux::FCrashDebugHelperLinux()
{
}

FCrashDebugHelperLinux::~FCrashDebugHelperLinux()
{
}

bool FCrashDebugHelperLinux::ParseCrashDump(const FString& InCrashDumpName, FCrashDebugInfo& OutCrashDebugInfo)
{
	return false;
}

bool FCrashDebugHelperLinux::CreateMinidumpDiagnosticReport( const FString& InCrashDumpName )
{
	return true;
}
