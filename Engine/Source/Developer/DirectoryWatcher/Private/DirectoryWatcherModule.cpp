// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DirectoryWatcherPrivatePCH.h"
#include "ModuleInterface.h"
#include "DirectoryWatcherModule.h"
#include "ModuleManager.h"

IMPLEMENT_MODULE( FDirectoryWatcherModule, DirectoryWatcher );
DEFINE_LOG_CATEGORY(LogDirectoryWatcher);

void FDirectoryWatcherModule::StartupModule()
{
	DirectoryWatcher = new FDirectoryWatcher();
}


void FDirectoryWatcherModule::ShutdownModule()
{
	if (DirectoryWatcher != NULL)
	{
		delete DirectoryWatcher;
		DirectoryWatcher = NULL;
	}
}

IDirectoryWatcher* FDirectoryWatcherModule::Get()
{
	return DirectoryWatcher;
}