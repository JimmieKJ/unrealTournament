// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperPrivatePCH.h"

IMPLEMENT_MODULE(FCrashDebugHelperModule, CrashDebugHelper);
DEFINE_LOG_CATEGORY(LogCrashDebugHelper);

#if PLATFORM_WINDOWS
	#include "CrashDebugHelperWindows.h"

#elif PLATFORM_LINUX
	#include "CrashDebugHelperLinux.h"

#elif PLATFORM_MAC
	#include "CrashDebugHelperMac.h"

#else
	#error "Unknown platform"
#endif

void FCrashDebugHelperModule::StartupModule()
{
	CrashDebugHelper = new FCrashDebugHelper();
	if (CrashDebugHelper != NULL)
	{
		CrashDebugHelper->Init();
	}
}

void FCrashDebugHelperModule::ShutdownModule()
{
	if (CrashDebugHelper != NULL)
	{
		delete CrashDebugHelper;
		CrashDebugHelper = NULL;
	}
}

ICrashDebugHelper* FCrashDebugHelperModule::Get()
{
	return CrashDebugHelper;
}
