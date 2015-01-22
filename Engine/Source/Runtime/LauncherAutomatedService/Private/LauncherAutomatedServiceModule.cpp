// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LauncherAutomatedServicePrivatePCH.h"
#include "ModuleManager.h"


IMPLEMENT_MODULE(FLauncherAutomatedServiceModule, LauncherAutomatedService);


ILauncherAutomatedServiceProviderPtr FLauncherAutomatedServiceModule::CreateAutomatedServiceProvider()
{
	return MakeShareable(new FLauncherAutomatedServiceProvider());
}
