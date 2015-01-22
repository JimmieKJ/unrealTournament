// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "RequiredProgramMainCPPInclude.h"
#include "ModuleManager.h"
#include "UE4EditorServicesAppDelegate.h"

#import <Cocoa/Cocoa.h>

IMPLEMENT_APPLICATION(UE4EditorServices, "UE4EditorServices");

int main(int argc, char *argv[])
{
	FCommandLine::Set(TEXT(""));
	FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();
	SCOPED_AUTORELEASE_POOL;
	[NSApplication sharedApplication];
	[NSApp setDelegate:[FUE4EditorServicesAppDelegate new]];
	[NSApp run];
	return 0;
}
