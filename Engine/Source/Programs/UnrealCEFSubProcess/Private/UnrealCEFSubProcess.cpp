// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//
// UnrealCEFSubProcess.cpp : Defines the entry point for the browser sub process
//

#include "UnrealCEFSubProcess.h"

#include "RequiredProgramMainCPPInclude.h"

#include "CEF3Utils.h"

//#define DEBUG_USING_CONSOLE	0

IMPLEMENT_APPLICATION(UnrealCEFSubProcess, "UnrealCEFSubProcess")

#if WITH_CEF3
int32 RunCEFSubProcess(const CefMainArgs& MainArgs)
{
	CEF3Utils::LoadCEF3Modules();
	// Execute the sub-process logic. This will block until the sub-process should exit.
	int32 Result = CefExecuteProcess(MainArgs, nullptr, nullptr);
	CEF3Utils::UnloadCEF3Modules();
	return Result;
}
#endif
