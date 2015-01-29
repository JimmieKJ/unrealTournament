// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SampleMutator.h"

#include "Core.h"
#include "Engine.h"
#include "ModuleManager.h"
#include "ModuleInterface.h"

class FSampleMutatorPlugin : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FSampleMutatorPlugin, SampleMutator )

void FSampleMutatorPlugin::StartupModule()
{
	
}


void FSampleMutatorPlugin::ShutdownModule()
{
	
}



