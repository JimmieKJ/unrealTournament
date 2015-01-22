// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperPrivatePCH.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "CrashDebugHelperModule.h"

IMPLEMENT_MODULE(FCrashDebugHelperModule, CrashDebugHelper);
DEFINE_LOG_CATEGORY(LogCrashDebugHelper);

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
