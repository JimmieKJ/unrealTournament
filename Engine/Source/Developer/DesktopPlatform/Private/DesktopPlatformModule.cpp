// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DesktopPlatformPrivatePCH.h"
#include "DesktopPlatformModule.h"

IMPLEMENT_MODULE( FDesktopPlatformModule, DesktopPlatform );
DEFINE_LOG_CATEGORY(LogDesktopPlatform);

void FDesktopPlatformModule::StartupModule()
{
	DesktopPlatform = new FDesktopPlatform();
}

void FDesktopPlatformModule::ShutdownModule()
{
	if (DesktopPlatform != NULL)
	{
		delete DesktopPlatform;
		DesktopPlatform = NULL;
	}
}