// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Char.h"
#include "CString.h"

#pragma once

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
/** Needed for the console command "DumpConsoleCommands" */
CORE_API void ConsoleCommandLibrary_DumpLibrary(class UWorld* InWorld, FExec& SubSystem, const FString& Pattern, FOutputDevice& Ar);
/** Needed for the console command "Help" */
CORE_API void ConsoleCommandLibrary_DumpLibraryHTML(class UWorld* InWorld, FExec& SubSystem, const FString& OutPath);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

namespace StringUtility
{
	/**
	 * Unescapes a URI
	 *
	 * @param URLString an escaped string (e.g. File%20Name)
	 *
	 * @return un-escaped string (e.g. "File Name")
	 */
	FString CORE_API UnescapeURI(const FString& URLString);
}

