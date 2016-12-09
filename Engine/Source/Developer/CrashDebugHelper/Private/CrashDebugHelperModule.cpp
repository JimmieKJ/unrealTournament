// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperModule.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "CrashDebugHelperPrivate.h"

IMPLEMENT_MODULE(FCrashDebugHelperModule, CrashDebugHelper);
DEFINE_LOG_CATEGORY(LogCrashDebugHelper);

#if PLATFORM_WINDOWS
	#include "CrashDebugHelperWindows.h"

#elif PLATFORM_LINUX
	#include "Linux/CrashDebugHelperLinux.h"

#elif PLATFORM_MAC
	#include "CrashDebugHelperMac.h"

#else
	#error "Unknown platform"
#endif

void FCrashDebugHelperModule::StartupModule()
{
	CrashDebugHelper = new FCrashDebugHelper();
	if (CrashDebugHelper != nullptr)
	{
		CrashDebugHelper->Init();
	}
}

void FCrashDebugHelperModule::ShutdownModule()
{
	if (CrashDebugHelper != nullptr)
	{
		delete CrashDebugHelper;
		CrashDebugHelper = nullptr;
	}
}

ICrashDebugHelper* FCrashDebugHelperModule::Get()
{
	return CrashDebugHelper;
}
