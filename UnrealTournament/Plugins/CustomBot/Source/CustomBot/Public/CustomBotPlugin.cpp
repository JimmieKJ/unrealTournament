// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CustomBotPrivatePCH.h"

#include "Core.h"
#include "Engine.h"
#include "ModuleManager.h"
#include "ModuleInterface.h"

class FCustomBotPlugin : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FCustomBotPlugin, CustomBot)

void FCustomBotPlugin::StartupModule()
{

}


void FCustomBotPlugin::ShutdownModule()
{

}



