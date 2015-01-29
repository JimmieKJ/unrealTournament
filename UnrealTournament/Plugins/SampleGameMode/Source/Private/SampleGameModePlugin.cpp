// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SampleGameMode.h"

#include "Core.h"
#include "Engine.h"
#include "ModuleManager.h"
#include "ModuleInterface.h"

class FSampleGameModePlugin : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FSampleGameModePlugin, SampleGameMode )

void FSampleGameModePlugin::StartupModule()
{
	
}


void FSampleGameModePlugin::ShutdownModule()
{
	
}



