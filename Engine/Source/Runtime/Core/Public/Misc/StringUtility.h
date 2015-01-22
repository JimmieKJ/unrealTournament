// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Char.h"
#include "CString.h"

#pragma once

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
/** Needed for the console command "DumpConsoleCommands" */
CORE_API void ConsoleCommandLibrary_DumpLibrary(class UWorld* InWorld, FExec& SubSystem, const FString& Pattern, FOutputDevice& Ar);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
